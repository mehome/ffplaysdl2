#pragma once
//#include "config.h"
#define CONFIG_AVDEVICE 0
#define CONFIG_AVFILTER 0
#define CONFIG_RTSP_DEMUXER 0
#define CONFIG_MMSH_PROTOCOL 0
//#define DEBUG 1
#include "IOHelper.h"
#include <map>
extern "C"
{

#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>

#include "libavutil/avstring.h"
#include "libavutil/colorspace.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"

#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"



#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
	//#include <SDL.h>
	//#include <SDL_thread.h>


#include "cmdutils.h"
};


#define ARGB(a, r, g, b) (((a)<<24)|((r)<<16)|((g)<<8)|(b))
#define  av_gettime_relative av_gettime

#ifndef INT64_MIN	
#define INT64_MIN       (-0x7fffffffffffffffLL-1)
#endif

#ifndef INT64_MAX
#define INT64_MAX INT64_C(9223372036854775807)
#endif


#include <assert.h>

const AVRational g_base_time =  {1, AV_TIME_BASE}; // g_base_time;

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_FRAMES 5

/* Minimum SDL audio buffer size, in samples. */
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

/* no AV sync correction is done if below the minimum AV sync threshold */
#define AV_SYNC_THRESHOLD_MIN 0.04
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 0.1
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/* external clock speed adjustment constants for realtime sources based on buffer fullness */
#define EXTERNAL_CLOCK_SPEED_MIN  0.900
#define EXTERNAL_CLOCK_SPEED_MAX  1.010
#define EXTERNAL_CLOCK_SPEED_STEP 0.001

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

/* polls for possible required scr refresh at least this often, should be less than 1/fps */
#define REFRESH_RATE 0.01

/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
/* TODO: We assume that a decoded and resampled frame fits into this buffer */
#define SAMPLE_ARRAY_SIZE (8 * 65536)

#define CURSOR_HIDE_DELAY 1000000


/*AVRational make_AVREX(int num,int den)
{
	AVRational ret= {num,den};
	return ret;
}*/

 av_always_inline av_const int isnan(float x)
{
	uint32_t v = av_float2int(x);
	if ((v & 0x7f800000) != 0x7f800000)
		return 0;
	return v & 0x007fffff;
}


typedef struct MyAVPacketList {
    AVPacket pkt;
    struct MyAVPacketList *next;
    int serial;
} MyAVPacketList;

typedef struct PacketQueue {
    MyAVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int abort_request;
    int serial;
    SDL_mutex *mutex;
    SDL_cond *cond;
} PacketQueue;

#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE 9
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

typedef struct AudioParams {
    int freq;
    int channels;
    int64_t channel_layout;
    enum AVSampleFormat fmt;
    int frame_size;
    int bytes_per_sec;
} AudioParams;

typedef struct Clock {
    double pts;           /* clock base */
    double pts_drift;     /* clock base minus time at which we updated the clock */
    double last_updated;
    double speed;
    int serial;           /* clock is based on a packet with this serial */
    int paused;
    int *queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */
} Clock;

/* Common struct for handling all types of decoded data and allocated render buffers. */
typedef struct Frame {
    AVFrame *frame;
    AVSubtitle sub;
    int serial;
    double pts;           /* presentation timestamp for the frame */
    double duration;      /* estimated duration of the frame */
    int64_t pos;          /* byte position of the frame in the input file */
    SDL_Texture *bmp;
    int allocated;
    int reallocate;
    int width;
    int height;
    AVRational sar;
} Frame;

typedef struct FrameQueue {
    Frame queue[FRAME_QUEUE_SIZE];
    int rindex;
    int windex;
    int size;
    int max_size;
    int keep_last;
    int rindex_shown;
    SDL_mutex *mutex;
    SDL_cond *cond;
    PacketQueue *pktq;
} FrameQueue;

enum {
    AV_SYNC_AUDIO_MASTER, /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};


typedef struct Decoder {
    AVPacket pkt;
    AVPacket pkt_temp;
    PacketQueue *queue;
    AVCodecContext *avctx;
    int pkt_serial;
    int finished;
    int flushed;
    int packet_pending;
    SDL_cond *empty_queue_cond;
    int64_t start_pts;
    AVRational start_pts_tb;
    int64_t next_pts;
    AVRational next_pts_tb;
} Decoder;

typedef enum ShowMode {
	SHOW_MODE_NONE = -1, SHOW_MODE_VIDEO = 0, SHOW_MODE_WAVES, SHOW_MODE_RDFT, SHOW_MODE_NB
} ShowMode;

typedef struct VideoState {
    SDL_Thread *read_tid;
    SDL_Thread *video_tid;
    SDL_Thread *audio_tid;
    AVInputFormat *iformat;
    int no_background;
    int abort_request;
    int force_refresh;
    int paused;
    int last_paused;
    int queue_attachments_req;
    int seek_req;
    int seek_flags;
    int64_t seek_pos;
    int64_t seek_rel;
    int read_pause_return;
    AVFormatContext *ic;
    int realtime;

    Clock audclk;
    Clock vidclk;
    Clock extclk;

    FrameQueue pictq;
    FrameQueue subpq;
    FrameQueue sampq;

    Decoder auddec;
    Decoder viddec;
    Decoder subdec;

    int audio_stream;

    int av_sync_type;

    double audio_clock;
    int audio_clock_serial;
    double audio_diff_cum; /* used for AV difference average computation */
    double audio_diff_avg_coef;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    AVStream *audio_st;
    PacketQueue audioq;
    int audio_hw_buf_size;
    uint8_t silence_buf[SDL_AUDIO_MIN_BUFFER_SIZE];
    uint8_t *audio_buf;
    uint8_t *audio_buf1;
    unsigned int audio_buf_size; /* in bytes */
    unsigned int audio_buf1_size;
    int audio_buf_index; /* in bytes */
    int audio_write_buf_size;
    struct AudioParams audio_src;
#if CONFIG_AVFILTER
    struct AudioParams audio_filter_src;
#endif
    struct AudioParams audio_tgt;
    struct SwrContext *swr_ctx;
    int frame_drops_early;
    int frame_drops_late;

    /*enum ShowMode {
        SHOW_MODE_NONE = -1, SHOW_MODE_VIDEO = 0, SHOW_MODE_WAVES, SHOW_MODE_RDFT, SHOW_MODE_NB
    } show_mode;*/
	ShowMode  show_mode;
    int16_t sample_array[SAMPLE_ARRAY_SIZE];
    int sample_array_index;
    int last_i_start;
    RDFTContext *rdft;
    int rdft_bits;
    FFTSample *rdft_data;
    int xpos;
    double last_vis_time;

    SDL_Thread *subtitle_tid;
    int subtitle_stream;
    AVStream *subtitle_st;
    PacketQueue subtitleq;

    double frame_timer;
    double frame_last_returned_time;
    double frame_last_filter_delay;
    int video_stream;
    AVStream *video_st;
    PacketQueue videoq;
    double max_frame_duration;      // maximum duration of a frame - above this, we consider the jump a timestamp discontinuity
#if !CONFIG_AVFILTER
    struct SwsContext *img_convert_ctx;
#endif
    SDL_Rect last_display_rect;

    char filename[1024];
    int width, height, xleft, ytop;
    int step;

#if CONFIG_AVFILTER
    int vfilter_idx;
    AVFilterContext *in_video_filter;   // the first filter in the video chain
    AVFilterContext *out_video_filter;  // the last filter in the video chain
    AVFilterContext *in_audio_filter;   // the first filter in the audio chain
    AVFilterContext *out_audio_filter;  // the last filter in the audio chain
    AVFilterGraph *agraph;              // audio filter graph
#endif

    int last_video_stream, last_audio_stream, last_subtitle_stream;

    SDL_cond *continue_read_thread;
} VideoState;

#define RGBA_IN(r, g, b, a, s)\
{\
	unsigned int v = ((const uint32_t *)(s))[0];\
	a = (v >> 24) & 0xff;\
	r = (v >> 16) & 0xff;\
	g = (v >> 8) & 0xff;\
	b = v & 0xff;\
}

#define YUVA_OUT(d, y, u, v, a)\
{\
	((uint32_t *)(d))[0] = (a << 24) | (y << 16) | (u << 8) | v;\
}

#define BPP 1

#define FF_ALLOC_EVENT   (SDL_USEREVENT)
#define FF_QUIT_EVENT    (SDL_USEREVENT + 2)
#define FF_FULLSCREEN    (SDL_USEREVENT + 3)


class CMedia
{
public:
	CMedia(void);
	~CMedia(void);
public:
	friend int audio_thread(void *arg);
	friend int video_thread(void *arg);
	friend int subtitle_thread(void *arg);
	friend void sdl_audio_callback(void *opaque, Uint8 *stream, int len);
	friend int read_thread(void *arg);
	friend int event_loop(void * arg);
	int OpenFile(char * filename);
	void SetHWND(HWND param1 );
private:
	void free_picture(Frame *vp);
	int packet_queue_put_private(PacketQueue *q, AVPacket *pkt);
	int packet_queue_put(PacketQueue *q, AVPacket *pkt);
	int packet_queue_put_nullpacket(PacketQueue *q, int stream_index);
	void packet_queue_init(PacketQueue *q);
	void packet_queue_flush(PacketQueue *q);
	void packet_queue_abort(PacketQueue *q);
	void packet_queue_start(PacketQueue *q);
	int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial);
	void decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond) ;
	int decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub) ;
	void decoder_destroy(Decoder *d) ;
	void frame_queue_unref_item(Frame *vp);
	int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last);
	void frame_queue_destory(FrameQueue *f);
	void frame_queue_signal(FrameQueue *f);
	Frame *frame_queue_peek(FrameQueue *f);
	Frame *frame_queue_peek_next(FrameQueue *f);
	Frame *frame_queue_peek_last(FrameQueue *f);
	Frame *frame_queue_peek_writable(FrameQueue *f);
	Frame *frame_queue_peek_readable(FrameQueue *f);
	void frame_queue_push(FrameQueue *f);
	void frame_queue_next(FrameQueue *f);
	int frame_queue_prev(FrameQueue *f);
	int frame_queue_nb_remaining(FrameQueue *f);
	int64_t frame_queue_last_pos(FrameQueue *f);
	inline void fill_rectangle(SDL_Surface *screen, int x, int y, int w, int h, int color, int update);
	inline double rint(double x);
	void calculate_display_rect(SDL_Rect *rect, int scr_xleft, int scr_ytop, int scr_width, int scr_height, int pic_width, int pic_height, AVRational pic_sar);
	void video_image_display(VideoState *is);
	inline int compute_mod(int a, int b);
	void video_audio_display(VideoState *s);
	void stream_close(VideoState *is);
	void do_exit(VideoState *is);
	void set_default_window_size(int width, int height, AVRational sar);
	void video_display(VideoState *is);
	double get_clock(Clock *c);
	void set_clock_at(Clock *c, double pts, int serial, double time);
	void set_clock(Clock *c, double pts, int serial);
	void set_clock_speed(Clock *c, double speed);
	void init_clock(Clock *c, int *queue_serial);
	void sync_clock_to_slave(Clock *c, Clock *slave);
	int get_master_sync_type(VideoState *is);
	double get_master_clock(VideoState *is);
	void check_external_clock_speed(VideoState *is);
	void stream_seek(VideoState *is, int64_t pos, int64_t rel, int seek_by_bytes);
	void stream_toggle_pause(VideoState *is);
	void toggle_pause(VideoState *is);
	void step_to_next_frame(VideoState *is);
	double compute_target_delay(double delay, VideoState *is);
	double vp_duration(VideoState *is, Frame *vp, Frame *nextvp);
	void update_video_pts(VideoState *is, double pts, int64_t pos, int serial);
	void video_refresh(void *opaque, double *remaining_time);
	int do_scale_picture(VideoState *is, Frame *vp, AVFrame *src_frame);
	void alloc_picture(VideoState *is, AVFrame *src);
	int queue_picture(VideoState *is, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial);
	int get_video_frame(VideoState *is, AVFrame *frame);
	void update_sample_display(VideoState *is, short *samples, int samples_size);
	int synchronize_audio(VideoState *is, int nb_samples);
	int audio_decode_frame(VideoState *is);
	int audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params);
	int stream_component_open(VideoState *is, int stream_index);
	void stream_component_close(VideoState *is, int stream_index);
	int is_realtime(AVFormatContext *s);
	VideoState *stream_open(const char *filename, AVInputFormat *iformat);
	
	void toggle_audio_display(VideoState *is);
	void refresh_loop_wait_event(VideoState *is, SDL_Event *event);
	void seek_chapter(VideoState *is, int incr);
	void stream_cycle_channel(VideoState *is, int codec_type);
	void packet_queue_destroy(PacketQueue *q);
	int video_open(VideoState *is, int force_set_video_mode, Frame *vp);
	void DoSeek(double dInc);
public:
	void toggle_full_screen(VideoState *is);
	VideoState * GetIs()
	{
		return m_vsData;
	}
public:
	bool bStopDraw;
private:

	VideoState *m_vsData;
	/* options specified by the user */
	 AVInputFormat *file_iformat;
	 int dummy;
	 const char *window_title;
	 int fs_screen_width;
	 int fs_screen_height;
	 int default_width  ;
	 int default_height ;
	 int screen_width  ;
	 int screen_height ;
	 int audio_disable;
	 int video_disable;
	 int subtitle_disable;
	 int wanted_stream[AVMEDIA_TYPE_NB];
	//{
	//    [AVMEDIA_TYPE_AUDIO]    = -1,
	//    [AVMEDIA_TYPE_VIDEO]    = -1,
	//    [AVMEDIA_TYPE_SUBTITLE] = -1,
	//};
	 int seek_by_bytes;
	 int display_disable;
	 int show_status ;
	 int av_sync_type;
	 int64_t start_time;
	 int64_t duration ;
	 int64_t sws_flags;
	 int fast ;
	 int genpts ;
	 int lowres ;
	 int decoder_reorder_pts ;
	 int autoexit;
	 int exit_on_keydown;
	 int exit_on_mousedown;
	 int loop ;
	 int framedrop ;
	 int infinite_buffer ;
	 enum ShowMode show_mode ;
	 char *audio_codec_name;
	 char *subtitle_codec_name;
	 char *video_codec_name;
	double rdftspeed ;
	 int64_t cursor_last_shown;
	 int cursor_hidden ;
#if CONFIG_AVFILTER
	 const char **vfilters_list = NULL;
	 int nb_vfilters = 0;
	 char *afilters = NULL;
#endif
	 int autorotate ;

	/* current context */
	 int is_full_screen;
	 int64_t audio_callback_time;

	 AVPacket flush_pkt;
	 char *input_filename;
	 HWND m_hWnd;

	 SDL_Window *win;
	 SDL_Renderer *render;
	 SDL_Surface *screen; /*dumpy*/
	 static int nWindowId;
	 int nNOWId;
public:
	 static std::map <void * ,int> media_map;


};

