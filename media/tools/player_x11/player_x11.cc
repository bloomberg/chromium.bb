// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <signal.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "media/base/filter_collection.h"
#include "media/base/media.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/message_loop_factory_impl.h"
#include "media/base/pipeline_impl.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer_factory.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/file_data_source_factory.h"
#include "media/filters/null_audio_renderer.h"
#include "media/filters/reference_audio_renderer.h"
#include "media/tools/player_x11/gl_video_renderer.h"
#include "media/tools/player_x11/x11_video_renderer.h"

static Display* g_display = NULL;
static Window g_window = 0;
static bool g_running = false;

class MessageLoopQuitter {
 public:
  explicit MessageLoopQuitter(MessageLoop* loop) : loop_(loop) {}
  void Quit(media::PipelineStatus status) {
    loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    delete this;
  }
 private:
  MessageLoop* loop_;
  DISALLOW_COPY_AND_ASSIGN(MessageLoopQuitter);
};

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

bool InitPipeline(MessageLoop* message_loop,
                  const char* filename,
                  bool use_gl,
                  bool enable_audio,
                  scoped_refptr<media::PipelineImpl>* pipeline,
                  MessageLoop* paint_message_loop,
                  media::MessageLoopFactory* message_loop_factory) {
  // Load media libraries.
  if (!media::InitializeMediaLibrary(FilePath())) {
    std::cout << "Unable to initialize the media library." << std::endl;
    return false;
  }

  // Create our filter factories.
  scoped_ptr<media::FilterCollection> collection(
      new media::FilterCollection());
  collection->SetDemuxerFactory(
      new media::FFmpegDemuxerFactory(
          new media::FileDataSourceFactory(), message_loop));
  collection->AddAudioDecoder(new media::FFmpegAudioDecoder(
      message_loop_factory->GetMessageLoop("AudioDecoderThread")));
  collection->AddVideoDecoder(new media::FFmpegVideoDecoder(
      message_loop_factory->GetMessageLoop("VideoDecoderThread")));

  if (use_gl) {
    collection->AddVideoRenderer(
        new GlVideoRenderer(g_display, g_window, paint_message_loop));
  } else {
    collection->AddVideoRenderer(
        new X11VideoRenderer(g_display, g_window, paint_message_loop));
  }

  if (enable_audio)
    collection->AddAudioRenderer(new media::ReferenceAudioRenderer());
  else
    collection->AddAudioRenderer(new media::NullAudioRenderer());

  // Create the pipeline and start it.
  *pipeline = new media::PipelineImpl(message_loop, new media::MediaLog());
  media::PipelineStatusNotification note;
  (*pipeline)->Start(collection.release(), filename, note.Callback());

  // Wait until the pipeline is fully initialized.
  note.Wait();
  if (note.status() != media::PIPELINE_OK) {
    std::cout << "InitPipeline: " << note.status() << std::endl;
    (*pipeline)->Stop(media::PipelineStatusCB());
    return false;
  }

  // And start the playback.
  (*pipeline)->SetPlaybackRate(1.0f);
  return true;
}

void TerminateHandler(int signal) {
  g_running = false;
}

void PeriodicalUpdate(
    media::PipelineImpl* pipeline,
    MessageLoop* message_loop,
    bool audio_only) {
  if (!g_running) {
    // interrupt signal was received during last time period.
    // Quit message_loop only when pipeline is fully stopped.
    MessageLoopQuitter* quitter = new MessageLoopQuitter(message_loop);
    pipeline->Stop(base::Bind(&MessageLoopQuitter::Quit,
                              base::Unretained(quitter)));
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
          KeySym key = XKeycodeToKeysym(g_display, e.xkey.keycode, 0);
          if (key == XK_Escape) {
            g_running = false;
            // Quit message_loop only when pipeline is fully stopped.
            MessageLoopQuitter* quitter = new MessageLoopQuitter(message_loop);
            pipeline->Stop(base::Bind(&MessageLoopQuitter::Quit,
                                      base::Unretained(quitter)));
            return;
          } else if (key == XK_space) {
            if (pipeline->GetPlaybackRate() < 0.01f) // paused
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

  message_loop->PostDelayedTask(FROM_HERE, base::Bind(
      &PeriodicalUpdate, make_scoped_refptr(pipeline),
      message_loop, audio_only), 10);
}

int main(int argc, char** argv) {
  // Read arguments.
  if (argc == 1) {
    std::cout << "Usage: " << argv[0] << " --file=FILE" << std::endl
              << std::endl
              << "Optional arguments:" << std::endl
              << "  [--audio]"
              << "  [--alsa-device=DEVICE]"
              << "  [--use-gl]" << std::endl
              << " Press [ESC] to stop" << std::endl
              << " Press [SPACE] to toggle pause/play" << std::endl
              << " Press mouse left button to seek" << std::endl;
    return 1;
  }

  // Read command line.
  CommandLine::Init(argc, argv);
  std::string filename =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII("file");
  bool enable_audio = CommandLine::ForCurrentProcess()->HasSwitch("audio");
  bool use_gl = CommandLine::ForCurrentProcess()->HasSwitch("use-gl");
  bool audio_only = false;

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
  base::AtExitManager at_exit;
  scoped_ptr<media::MessageLoopFactory> message_loop_factory(
      new media::MessageLoopFactoryImpl());
  scoped_ptr<base::Thread> thread;
  scoped_refptr<media::PipelineImpl> pipeline;
  MessageLoop message_loop;
  thread.reset(new base::Thread("PipelineThread"));
  thread->Start();
  if (InitPipeline(thread->message_loop(), filename.c_str(),
                   use_gl, enable_audio, &pipeline, &message_loop,
                   message_loop_factory.get())) {
    // Main loop of the application.
    g_running = true;

    // Check if video is present.
    audio_only = !pipeline->HasVideo();

    message_loop.PostTask(FROM_HERE, base::Bind(
        &PeriodicalUpdate, pipeline, &message_loop, audio_only));
    message_loop.Run();
  } else{
    std::cout << "Pipeline initialization failed..." << std::endl;
  }

  // Cleanup tasks.
  message_loop_factory.reset();

  thread->Stop();
  XDestroyWindow(g_display, g_window);
  XCloseDisplay(g_display);
  return 0;
}
