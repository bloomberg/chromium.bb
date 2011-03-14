// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <signal.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "media/base/callback.h"
#include "media/base/filter_collection.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/base/message_loop_factory_impl.h"
#include "media/base/pipeline_impl.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer_factory.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/file_data_source_factory.h"
#include "media/filters/null_audio_renderer.h"
#include "media/filters/omx_video_decoder.h"

// TODO(jiesun): implement different video decode contexts according to
// these flags. e.g.
//     1.  system memory video decode context for x11
//     2.  gl texture video decode context for OpenGL.
//     3.  gles texture video decode context for OpenGLES.
// TODO(jiesun): add an uniform video renderer which take the video
//       decode context object and delegate renderer request to these
//       objects. i.e. Seperate "painter" and "pts scheduler".
#if defined(RENDERER_GL)
#include "media/tools/player_x11/gl_video_renderer.h"
typedef GlVideoRenderer Renderer;
#elif defined(RENDERER_GLES)
#include "media/tools/player_x11/gles_video_renderer.h"
typedef GlesVideoRenderer Renderer;
#elif defined(RENDERER_X11)
#include "media/tools/player_x11/x11_video_renderer.h"
typedef X11VideoRenderer Renderer;
#else
#error No video renderer defined.
#endif

Display* g_display = NULL;
Window g_window = 0;
bool g_running = false;

void Quit(MessageLoop* message_loop) {
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
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

bool InitPipeline(MessageLoop* message_loop,
                  const char* filename, bool enable_audio,
                  scoped_refptr<media::PipelineImpl>* pipeline,
                  MessageLoop* paint_message_loop,
                  media::MessageLoopFactory* message_loop_factory) {
  // Initialize OpenMAX.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableOpenMax) &&
      !media::InitializeOpenMaxLibrary(FilePath())) {
    std::cout << "Unable to initialize OpenMAX library."<< std::endl;
    return false;
  }

  // Load media libraries.
  if (!media::InitializeMediaLibrary(FilePath())) {
    std::cout << "Unable to initialize the media library." << std::endl;
    return false;
  }

  // Create our filter factories.
  scoped_ptr<media::FilterCollection> collection(
      new media::FilterCollection());
  collection->SetDemuxerFactory(new media::FFmpegDemuxerFactory(
      new media::FileDataSourceFactory(), message_loop));
  collection->AddAudioDecoder(new media::FFmpegAudioDecoder(
      message_loop_factory->GetMessageLoop("AudioDecoderThread")));
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableOpenMax)) {
    collection->AddVideoDecoder(new media::OmxVideoDecoder(
        message_loop_factory->GetMessageLoop("VideoDecoderThread"),
        NULL));
  } else {
    collection->AddVideoDecoder(new media::FFmpegVideoDecoder(
        message_loop_factory->GetMessageLoop("VideoDecoderThread"),
        NULL));
  }
  collection->AddVideoRenderer(new Renderer(g_display,
                                            g_window,
                                            paint_message_loop));

  if (enable_audio)
    collection->AddAudioRenderer(new media::AudioRendererImpl());
  else
    collection->AddAudioRenderer(new media::NullAudioRenderer());

  // Create and start the pipeline.
  *pipeline = new media::PipelineImpl(message_loop);
  (*pipeline)->Start(collection.release(), filename, NULL);

  // Wait until the pipeline is fully initialized.
  while (true) {
    base::PlatformThread::Sleep(100);
    if ((*pipeline)->IsInitialized())
      break;
    if ((*pipeline)->GetError() != media::PIPELINE_OK) {
      std::cout << "InitPipeline: " << (*pipeline)->GetError() << std::endl;
      (*pipeline)->Stop(NULL);
      return false;
    }
  }

  // And starts the playback.
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
    // interrupt signal is received during lat time period.
    // Quit message_loop only when pipeline is fully stopped.
    pipeline->Stop(media::TaskToCallbackAdapter::NewCallback(
        NewRunnableFunction(Quit, message_loop)));
    return;
  }

  // Consume all the X events
  while (XPending(g_display)) {
    XEvent e;
    XNextEvent(g_display, &e);
    switch (e.type) {
      case Expose:
        if (!audio_only) {
          // Tell the renderer to paint.
          DCHECK(Renderer::instance());
          Renderer::instance()->Paint();
        }
        break;
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
          pipeline->Seek(time*e.xbutton.x/width, NULL);
        }
        break;
      case KeyPress:
        {
          KeySym key = XKeycodeToKeysym(g_display, e.xkey.keycode, 0);
          if (key == XK_Escape) {
            g_running = false;
            // Quit message_loop only when pipeline is fully stopped.
            pipeline->Stop(media::TaskToCallbackAdapter::NewCallback(
                NewRunnableFunction(Quit, message_loop)));
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

  message_loop->PostDelayedTask(FROM_HERE,
      NewRunnableFunction(PeriodicalUpdate, make_scoped_refptr(pipeline),
                          message_loop, audio_only), 10);
}

int main(int argc, char** argv) {
  // Read arguments.
  if (argc == 1) {
    std::cout << "Usage: " << argv[0] << " --file=FILE" << std::endl
              << std::endl
              << "Optional arguments:" << std::endl
              << "  [--enable-openmax]"
              << "  [--audio]"
              << "  [--alsa-device=DEVICE]" << std::endl
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
  bool audio_only = false;

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
                   enable_audio, &pipeline, &message_loop,
                   message_loop_factory.get())) {
    // Main loop of the application.
    g_running = true;

    // Check if video is present.
    audio_only = !pipeline->HasVideo();

    message_loop.PostTask(FROM_HERE,
        NewRunnableFunction(PeriodicalUpdate, pipeline,
                            &message_loop, audio_only));
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
