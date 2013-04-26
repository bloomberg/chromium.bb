// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>

#include <iostream>  // NOLINT

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "media/audio/audio_manager.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/decryptor.h"
#include "media/base/filter_collection.h"
#include "media/base/media.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline.h"
#include "media/base/video_frame.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/file_data_source.h"
#include "media/filters/video_renderer_base.h"
#include "media/tools/player_x11/data_source_logger.h"

// Include X11 headers here because X11/Xlib.h #define's Status
// which causes compiler errors with Status enum declarations
// in media::DemuxerStream & media::AudioDecoder.
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include "media/tools/player_x11/gl_video_renderer.h"
#include "media/tools/player_x11/x11_video_renderer.h"

static Display* g_display = NULL;
static Window g_window = 0;
static bool g_running = false;

media::AudioManager* g_audio_manager = NULL;

scoped_ptr<media::DataSource> CreateDataSource(const std::string& file_path) {
  media::FileDataSource* file_data_source = new media::FileDataSource();
  CHECK(file_data_source->Initialize(base::FilePath(file_path)));

  scoped_ptr<media::DataSource> data_source(file_data_source);
  return data_source.Pass();
}

// Initialize X11. Returns true if successful. This method creates the X11
// window. Further initialization is done in X11VideoRenderer.
bool InitX11() {
  g_display = XOpenDisplay(NULL);
  if (!g_display) {
    std::cout << "Error - cannot open display" << std::endl;
    return false;
  }

  // Get properties of the screen.
  int screen = DefaultScreen(g_display);
  int root_window = RootWindow(g_display, screen);

  // Creates the window.
  g_window = XCreateSimpleWindow(g_display, root_window, 1, 1, 100, 50, 0,
                                 BlackPixel(g_display, screen),
                                 BlackPixel(g_display, screen));
  XStoreName(g_display, g_window, "X11 Media Player");

  XSelectInput(g_display, g_window,
               ExposureMask | ButtonPressMask | KeyPressMask);
  XMapWindow(g_display, g_window);
  return true;
}

void SetOpaque(bool /*opaque*/) {
}

typedef base::Callback<void(media::VideoFrame*)> PaintCB;
void Paint(base::MessageLoop* message_loop, const PaintCB& paint_cb,
           const scoped_refptr<media::VideoFrame>& video_frame) {
  if (message_loop != base::MessageLoop::current()) {
    message_loop->PostTask(FROM_HERE, base::Bind(
        &Paint, message_loop, paint_cb, video_frame));
    return;
  }

  paint_cb.Run(video_frame);
}

static void OnBufferingState(media::Pipeline::BufferingState buffering_state) {}

static void NeedKey(const std::string& type, scoped_ptr<uint8[]> init_data,
             int init_data_size) {
  std::cout << "File is encrypted." << std::endl;
}

static void SaveStatusAndSignal(base::WaitableEvent* event,
                                media::PipelineStatus* status_out,
                                media::PipelineStatus status) {
  *status_out = status;
  event->Signal();
}

// TODO(vrk): Re-enabled audio. (crbug.com/112159)
void InitPipeline(const scoped_refptr<base::MessageLoopProxy>& message_loop,
                  media::Demuxer* demuxer,
                  const PaintCB& paint_cb,
                  bool /* enable_audio */,
                  scoped_refptr<media::Pipeline>* pipeline,
                  base::MessageLoop* paint_message_loop) {
  // Create our filter factories.
  scoped_ptr<media::FilterCollection> collection(
      new media::FilterCollection());
  collection->SetDemuxer(demuxer);

  ScopedVector<media::VideoDecoder> video_decoders;
  video_decoders.push_back(new media::FFmpegVideoDecoder(message_loop));
  scoped_ptr<media::VideoRenderer> video_renderer(new media::VideoRendererBase(
      message_loop,
      video_decoders.Pass(),
      media::SetDecryptorReadyCB(),
      base::Bind(&Paint, paint_message_loop, paint_cb),
      base::Bind(&SetOpaque),
      true));
  collection->SetVideoRenderer(video_renderer.Pass());

  ScopedVector<media::AudioDecoder> audio_decoders;
  audio_decoders.push_back(new media::FFmpegAudioDecoder(message_loop));
  scoped_ptr<media::AudioRenderer> audio_renderer(new media::AudioRendererImpl(
      message_loop,
      new media::NullAudioSink(message_loop),
      audio_decoders.Pass(),
      media::SetDecryptorReadyCB()));
  collection->SetAudioRenderer(audio_renderer.Pass());

  // Create the pipeline and start it.
  base::WaitableEvent event(true, false);
  media::PipelineStatus status;

  *pipeline = new media::Pipeline(message_loop, new media::MediaLog());
  (*pipeline)->Start(
      collection.Pass(), base::Closure(), media::PipelineStatusCB(),
      base::Bind(&SaveStatusAndSignal, &event, &status),
      base::Bind(&OnBufferingState), base::Closure());

  // Wait until the pipeline is fully initialized.
  event.Wait();
  CHECK_EQ(status, media::PIPELINE_OK) << "Pipeline initialization failed";

  // And start the playback.
  (*pipeline)->SetPlaybackRate(1.0f);
}

void TerminateHandler(int signal) {
  g_running = false;
}

void PeriodicalUpdate(
    media::Pipeline* pipeline,
    base::MessageLoop* message_loop,
    bool audio_only) {
  if (!g_running) {
    // interrupt signal was received during last time period.
    // Quit message_loop only when pipeline is fully stopped.
    pipeline->Stop(base::MessageLoop::QuitClosure());
    return;
  }

  // Consume all the X events
  while (XPending(g_display)) {
    XEvent e;
    XNextEvent(g_display, &e);
    switch (e.type) {
      case ButtonPress:
        {
          Window window;
          int x, y;
          unsigned int width, height, border_width, depth;
          XGetGeometry(g_display,
                       g_window,
                       &window,
                       &x,
                       &y,
                       &width,
                       &height,
                       &border_width,
                       &depth);
          base::TimeDelta time = pipeline->GetMediaDuration();
          pipeline->Seek(time*e.xbutton.x/width, media::PipelineStatusCB());
        }
        break;
      case KeyPress:
        {
          KeySym key = XkbKeycodeToKeysym(g_display, e.xkey.keycode, 0, 0);
          if (key == XK_Escape) {
            g_running = false;
            // Quit message_loop only when pipeline is fully stopped.
            pipeline->Stop(base::MessageLoop::QuitClosure());
            return;
          } else if (key == XK_space) {
            if (pipeline->GetPlaybackRate() < 0.01f)  // paused
              pipeline->SetPlaybackRate(1.0f);
            else
              pipeline->SetPlaybackRate(0.0f);
          }
        }
        break;
      default:
        break;
    }
  }

  message_loop->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PeriodicalUpdate,
                 make_scoped_refptr(pipeline),
                 message_loop,
                 audio_only),
      base::TimeDelta::FromMilliseconds(10));
}

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  media::InitializeMediaLibraryForTesting();

  CommandLine::Init(argc, argv);
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  std::string filename = command_line->GetSwitchValueASCII("file");

  if (filename.empty()) {
    std::cout << "Usage: " << argv[0] << " --file=FILE" << std::endl
              << std::endl
              << "Optional arguments:" << std::endl
              << "  [--audio]"
              << "  [--alsa-device=DEVICE]"
              << "  [--use-gl]"
              << "  [--streaming]" << std::endl
              << " Press [ESC] to stop" << std::endl
              << " Press [SPACE] to toggle pause/play" << std::endl
              << " Press mouse left button to seek" << std::endl;
    return 1;
  }

  scoped_ptr<media::AudioManager> audio_manager(media::AudioManager::Create());
  g_audio_manager = audio_manager.get();

  logging::InitLogging(
      NULL,
      logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,  // Ignored.
      logging::DELETE_OLD_LOG_FILE,  // Ignored.
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  // Install the signal handler.
  signal(SIGTERM, &TerminateHandler);
  signal(SIGINT, &TerminateHandler);

  // Initialize X11.
  if (!InitX11())
    return 1;

  // Initialize the pipeline thread and the pipeline.
  base::MessageLoop message_loop;
  base::Thread media_thread("MediaThread");
  media_thread.Start();
  scoped_refptr<media::Pipeline> pipeline;

  PaintCB paint_cb;
  if (command_line->HasSwitch("use-gl")) {
    paint_cb = base::Bind(
        &GlVideoRenderer::Paint, new GlVideoRenderer(g_display, g_window));
  } else {
    paint_cb = base::Bind(
        &X11VideoRenderer::Paint, new X11VideoRenderer(g_display, g_window));
  }

  scoped_ptr<media::DataSource> data_source(new DataSourceLogger(
      CreateDataSource(filename), command_line->HasSwitch("streaming")));
  scoped_ptr<media::Demuxer> demuxer(new media::FFmpegDemuxer(
      media_thread.message_loop_proxy(), data_source.get(),
      base::Bind(&NeedKey)));

  InitPipeline(media_thread.message_loop_proxy(), demuxer.get(),
               paint_cb, command_line->HasSwitch("audio"), &pipeline,
               &message_loop);

  // Main loop of the application.
  g_running = true;

  message_loop.PostTask(FROM_HERE, base::Bind(
      &PeriodicalUpdate, pipeline, &message_loop, !pipeline->HasVideo()));
  message_loop.Run();

  // Cleanup tasks.
  media_thread.Stop();

  // Release callback which releases video renderer. Do this before cleaning up
  // X below since the video renderer has some X cleanup duties as well.
  paint_cb.Reset();

  XDestroyWindow(g_display, g_window);
  XCloseDisplay(g_display);
  g_audio_manager = NULL;

  return 0;
}
