// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include <iostream>
#include <signal.h>
#include <X11/Xlib.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline_impl.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/file_data_source.h"
#include "media/filters/null_audio_renderer.h"
#include "media/filters/omx_video_decoder.h"
#include "media/tools/player_x11/x11_video_renderer.h"

Display* g_display = NULL;
Window g_window = 0;
bool g_running = false;

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

  XSelectInput(g_display, g_window, ExposureMask | ButtonPressMask);
  XMapWindow(g_display, g_window);
  return true;
}

bool InitPipeline(MessageLoop* message_loop,
                  const char* filename, bool enable_audio,
                  scoped_refptr<media::PipelineImpl>* pipeline) {
  // Load media libraries.
  if (!media::InitializeMediaLibrary(FilePath())) {
    std::cout << "Unable to initialize the media library." << std::endl;
    return false;
  }

  // Create our filter factories.
  scoped_refptr<media::FilterFactoryCollection> factories =
      new media::FilterFactoryCollection();
  factories->AddFactory(media::FileDataSource::CreateFactory());
  factories->AddFactory(media::FFmpegAudioDecoder::CreateFactory());
  factories->AddFactory(media::FFmpegDemuxer::CreateFilterFactory());
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableOpenMax)) {
    factories->AddFactory(media::OmxVideoDecoder::CreateFactory());
  }
  factories->AddFactory(media::FFmpegVideoDecoder::CreateFactory());
  factories->AddFactory(X11VideoRenderer::CreateFactory(g_display, g_window));

  if (enable_audio) {
    factories->AddFactory(media::AudioRendererImpl::CreateFilterFactory());
  } else {
    factories->AddFactory(media::NullAudioRenderer::CreateFilterFactory());
  }

  // Creates the pipeline and start it.
  *pipeline = new media::PipelineImpl(message_loop);
  (*pipeline)->Start(factories, filename, NULL);

  // Wait until the pipeline is fully initialized.
  while (true) {
    PlatformThread::Sleep(100);
    if ((*pipeline)->IsInitialized())
      break;
    if ((*pipeline)->GetError() != media::PIPELINE_OK) {
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

int main(int argc, char** argv) {
  // Read arguments.
  if (argc == 1) {
    std::cout << "Usage: " << argv[0] << " --file=FILE [--audio]"
                 " [--alsa-device=DEVICE]" << std::endl;
    return 1;
  }

  // Read command line.
  CommandLine::Init(argc, argv);
  std::string filename =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII("file");
  bool enable_audio = CommandLine::ForCurrentProcess()->HasSwitch("audio");

  // Install the signal handler.
  signal(SIGTERM, &TerminateHandler);
  signal(SIGINT, &TerminateHandler);

  // Initialize X11.
  if (!InitX11())
    return 1;

  // Initialize the pipeline thread and the pipeline.
  base::AtExitManager at_exit;
  scoped_ptr<base::Thread> thread;
  scoped_refptr<media::PipelineImpl> pipeline;
  thread.reset(new base::Thread("PipelineThread"));
  thread->Start();
  if (InitPipeline(thread->message_loop(), filename.c_str(),
                   enable_audio, &pipeline)) {
    // Main loop of the application.
    g_running = true;
    while (g_running) {
      if (XPending(g_display)) {
        XEvent e;
        XNextEvent(g_display, &e);
        if (e.type == Expose) {
          // Tell the renderer to paint.
          DCHECK(X11VideoRenderer::instance());
          X11VideoRenderer::instance()->Paint();
        } else if (e.type == ButtonPress) {
          // Stop the playback.
          break;
        }
      } else {
        // If there's no event in the queue, make an expose event.
        XEvent event;
        event.type = Expose;
        XSendEvent(g_display, g_window, true, ExposureMask, &event);

        // TODO(hclam): It is rather arbitrary to sleep for 10ms and wait
        // for the next event. We should submit an expose event when
        // a frame is available but not firing an expose event every 10ms.
        usleep(10000);
      }
    }
    pipeline->Stop(NULL);
  } else{
    std::cout << "Pipeline initialization failed..." << std::endl;
  }

  // Cleanup tasks.
  thread->Stop();
  XDestroyWindow(g_display, g_window);
  XCloseDisplay(g_display);
  return 0;
}
