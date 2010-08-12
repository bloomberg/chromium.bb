// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Demonstrates the use of MftH264Decoder.

#include <cstdio>

#include <string>

#include <d3d9.h>
#include <dxva2api.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "media/base/media.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/file_protocol.h"
#include "media/mf/basic_renderer.h"
#include "media/mf/d3d_util.h"
#include "media/mf/file_reader_util.h"
#include "media/mf/mft_h264_decoder.h"

using base::AtExitManager;
using base::Time;
using base::TimeDelta;
using media::BasicRenderer;
using media::NullRenderer;
using media::FFmpegFileReader;
using media::MftH264Decoder;
using media::MftRenderer;
using media::VideoFrame;

namespace {

const wchar_t* const kWindowClass = L"Chrome_H264_MFT";
const wchar_t* const kWindowTitle = L"H264_MFT";
const int kWindowStyleFlags = (WS_OVERLAPPEDWINDOW | WS_VISIBLE) &
                              ~(WS_MAXIMIZEBOX | WS_THICKFRAME);

void usage() {
  static char* usage_msg =
      "Usage: mft_h264_decoder [--enable-dxva] [--render] --input-file=FILE\n"
      "enable-dxva: Enables hardware accelerated decoding\n"
      "render: Render to window\n"
      "During rendering, press spacebar to skip forward at least 5 seconds.\n"
      "To display this message: mft_h264_decoder --help";
  fprintf(stderr, "%s\n", usage_msg);
}

static bool InitFFmpeg() {
  if (!media::InitializeMediaLibrary(FilePath()))
    return false;
  avcodec_init();
  av_register_all();
  av_register_protocol2(&kFFmpegFileProtocol, sizeof(kFFmpegFileProtocol));
  return true;
}

bool InitComLibrary() {
  HRESULT hr;
  hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(hr)) {
    LOG(ERROR) << "CoInit fail";
    return false;
  }
  return true;
}

// Creates a window with the given width and height.
// Returns: A handle to the window on success, NULL otherwise.
static HWND CreateDrawWindow(int width, int height) {
  WNDCLASS window_class = {0};
  window_class.lpszClassName = kWindowClass;
  window_class.hInstance = NULL;
  window_class.hbrBackground = 0;
  window_class.lpfnWndProc = DefWindowProc;
  window_class.hCursor = 0;

  if (RegisterClass(&window_class) == 0) {
    LOG(ERROR) << "Failed to register window class";
    return false;
  }
  HWND window = CreateWindow(kWindowClass,
                             kWindowTitle,
                             kWindowStyleFlags,
                             100,
                             100,
                             width,
                             height,
                             NULL,
                             NULL,
                             NULL,
                             NULL);
  if (window == NULL) {
    LOG(ERROR) << "Failed to create window";
    return NULL;
  }
  return window;
}

class WindowObserver : public base::MessagePumpWin::Observer {
 public:
  WindowObserver(FFmpegFileReader* reader, MftH264Decoder* decoder)
      : reader_(reader),
        decoder_(decoder) {
  }

  virtual void WillProcessMessage(const MSG& msg) {
    if (msg.message == WM_CHAR && msg.wParam == ' ') {
      if (!decoder_->Flush()) {
        LOG(ERROR) << "Flush failed";
      }
      // Seek forward 5 seconds.
      reader_->SeekForward(5000000);
    }
  }

  virtual void DidProcessMessage(const MSG& msg) {
  }

 private:
  FFmpegFileReader* reader_;
  MftH264Decoder* decoder_;
};

static int Run(bool use_dxva, bool render, const std::string& input_file) {
  // If we are not rendering, we need a window anyway to create a D3D device,
  // so we will just use the desktop window. (?)
  HWND window = GetDesktopWindow();
  if (render) {
    window = CreateDrawWindow(640, 480);
    if (window == NULL) {
      LOG(ERROR) << "Failed to create window";
      return -1;
    }
  }
  scoped_ptr<FFmpegFileReader> reader(new FFmpegFileReader(input_file));
  if (reader.get() == NULL || !reader->Initialize()) {
    LOG(ERROR) << "Failed to create/initialize reader";
    return -1;
  }
  int width = 0, height = 0;
  if (!reader->GetWidth(&width) || !reader->GetHeight(&height)) {
    LOG(WARNING) << "Failed to get width/height from reader";
  }
  int aspect_ratio_num = 0, aspect_ratio_denom = 0;
  if (!reader->GetAspectRatio(&aspect_ratio_num, &aspect_ratio_denom)) {
    LOG(WARNING) << "Failed to get aspect ratio";
  }
  int frame_rate_num = 0, frame_rate_denom = 0;
  if (!reader->GetFrameRate(&frame_rate_num, &frame_rate_denom)) {
    LOG(WARNING) << "Failed to get frame rate";
  }
  ScopedComPtr<IDirect3D9> d3d9;
  ScopedComPtr<IDirect3DDevice9> device;
  ScopedComPtr<IDirect3DDeviceManager9> dev_manager;
  if (use_dxva) {
    dev_manager.Attach(media::CreateD3DDevManager(window,
                                                  d3d9.Receive(),
                                                  device.Receive()));
    if (dev_manager.get() == NULL) {
      LOG(ERROR) << "Cannot create D3D9 manager";
      return -1;
    }
  }
  scoped_refptr<MftH264Decoder> mft(new MftH264Decoder(use_dxva));
  scoped_refptr<MftRenderer> renderer;
  if (render) {
    renderer = new BasicRenderer(mft.get(), window, device);
  } else {
    renderer = new NullRenderer(mft.get());
  }
  if (mft.get() == NULL) {
    LOG(ERROR) << "Failed to create fake renderer / MFT";
    return -1;
  }
  if (!mft->Init(dev_manager,
                 frame_rate_num, frame_rate_denom,
                 width, height,
                 aspect_ratio_num, aspect_ratio_denom,
                 NewCallback(reader.get(), &FFmpegFileReader::Read),
                 NewCallback(renderer.get(), &MftRenderer::ProcessFrame),
                 NewCallback(renderer.get(),
                             &MftRenderer::OnDecodeError))) {
    LOG(ERROR) << "Failed to initialize mft";
    return -1;
  }
  scoped_ptr<WindowObserver> observer;
  // If rendering, resize the window to fit the video frames.
  if (render) {
    RECT rect;
    rect.left = 0;
    rect.right = mft->width();
    rect.top = 0;
    rect.bottom = mft->height();
    AdjustWindowRect(&rect, kWindowStyleFlags, FALSE);
    if (!MoveWindow(window, 0, 0, rect.right - rect.left,
                    rect.bottom - rect.top, TRUE)) {
      LOG(WARNING) << "Warning: Failed to resize window";
    }
    observer.reset(new WindowObserver(reader.get(), mft.get()));
    MessageLoopForUI::current()->AddObserver(observer.get());
  }
  if (use_dxva) {
    // Reset the device's back buffer dimensions to match the window's
    // dimensions.
    if (!media::AdjustD3DDeviceBackBufferDimensions(device.get(),
                                                    window,
                                                    mft->width(),
                                                    mft->height())) {
      LOG(WARNING) << "Warning: Failed to reset device to have correct "
                   << "backbuffer dimension, scaling might occur";
    }
  }
  Time decode_start(Time::Now());

  MessageLoopForUI::current()->PostTask(FROM_HERE,
      NewRunnableMethod(renderer.get(), &MftRenderer::StartPlayback));
  MessageLoopForUI::current()->Run(NULL);

  TimeDelta decode_time = Time::Now() - decode_start;

  printf("All done, frames read: %d, frames decoded: %d\n",
         mft->frames_read(), mft->frames_decoded());
  printf("Took %lldms\n", decode_time.InMilliseconds());
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  AtExitManager at_exit;
  MessageLoopForUI message_loop;
  CommandLine::Init(argc, argv);
  if (argc == 1) {
    fprintf(stderr, "Not enough arguments\n");
    usage();
    return -1;
  }
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  if (cmd_line.HasSwitch("help")) {
    usage();
    return -1;
  }
  bool use_dxva = cmd_line.HasSwitch("enable-dxva");
  bool render = cmd_line.HasSwitch("render");
  std::string input_file = cmd_line.GetSwitchValueASCII("input-file");
  if (input_file.empty()) {
    fprintf(stderr, "No input file provided\n");
    usage();
    return -1;
  }
  printf("enable-dxva: %d\n", use_dxva);
  printf("render: %d\n", render);
  printf("input-file: %s\n", input_file.c_str());

  if (!InitFFmpeg()) {
    LOG(ERROR) << "InitFFMpeg() failed";
    return -1;
  }
  if (!InitComLibrary()) {
    LOG(ERROR) << "InitComLibraries() failed";
    return -1;
  }
  int ret = Run(use_dxva, render, input_file);

  printf("Done\n");
  return ret;
}
