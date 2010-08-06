// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Demonstrates the use of MftH264Decoder.

#include <cstdio>

#include <string>

#include <d3d9.h>
#include <dxva2api.h>
#include <mfapi.h>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "media/base/media.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/file_protocol.h"
#include "media/mf/file_reader_util.h"
#include "media/mf/mft_h264_decoder.h"

using base::Time;
using base::TimeDelta;
using media::FFmpegFileReader;
using media::MftH264Decoder;
using media::VideoFrame;

namespace {

void usage() {
  static char* usage_msg =
      "Usage: mft_h264_decoder [--enable-dxva] --input-file=FILE\n"
      "enable-dxva: Enables hardware accelerated decoding\n"
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

bool InitComLibraries() {
  HRESULT hr;
  hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(hr)) {
    LOG(ERROR) << "CoInit fail";
    return false;
  }
  hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  if (FAILED(hr)) {
    LOG(ERROR) << "MFStartup fail";
    CoUninitialize();
    return false;
  }
  return true;
}

void ShutdownComLibraries() {
  HRESULT hr;
  hr = MFShutdown();
  if (FAILED(hr)) {
    LOG(WARNING) << "Warning: MF failed to shutdown";
  }
  CoUninitialize();
}

static IDirect3DDeviceManager9* CreateD3DDevManager(HWND video_window,
                                                    IDirect3D9** direct3d,
                                                    IDirect3DDevice9** device) {
  CHECK(video_window != NULL);
  CHECK(direct3d != NULL);
  CHECK(device != NULL);

  ScopedComPtr<IDirect3DDeviceManager9> dev_manager;
  ScopedComPtr<IDirect3D9> d3d;
  d3d.Attach(Direct3DCreate9(D3D_SDK_VERSION));
  if (d3d == NULL) {
    LOG(ERROR) << "Failed to create D3D9";
    return NULL;
  }
  D3DPRESENT_PARAMETERS present_params = {0};

  // Once we know the dimensions, we need to reset using
  // AdjustD3DDeviceBackBufferDimensions().
  present_params.BackBufferWidth = 0;
  present_params.BackBufferHeight = 0;
  present_params.BackBufferFormat = D3DFMT_UNKNOWN;
  present_params.BackBufferCount = 1;
  present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  present_params.hDeviceWindow = video_window;
  present_params.Windowed = TRUE;
  present_params.Flags = D3DPRESENTFLAG_VIDEO;
  present_params.FullScreen_RefreshRateInHz = 0;
  present_params.PresentationInterval = 0;

  ScopedComPtr<IDirect3DDevice9> temp_device;

  // D3DCREATE_HARDWARE_VERTEXPROCESSING specifies hardware vertex processing.
  // (Is it even needed for just video decoding?)
  HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT,
                                 D3DDEVTYPE_HAL,
                                 NULL,
                                 D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                 &present_params,
                                 temp_device.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create D3D Device";
    return NULL;
  }
  UINT dev_manager_reset_token = 0;
  hr = DXVA2CreateDirect3DDeviceManager9(&dev_manager_reset_token,
                                         dev_manager.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Couldn't create D3D Device manager";
    return NULL;
  }
  hr = dev_manager->ResetDevice(temp_device.get(), dev_manager_reset_token);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set device to device manager";
    return NULL;
  }
  *direct3d = d3d.Detach();
  *device = temp_device.Detach();
  return dev_manager.Detach();
}

static void ReleaseOutputBuffer(VideoFrame* frame) {
  if (frame != NULL &&
      frame->type() == VideoFrame::TYPE_MFBUFFER ||
      frame->type() == VideoFrame::TYPE_DIRECT3DSURFACE) {
    static_cast<IMFMediaBuffer*>(frame->private_buffer())->Release();
  }
}

class FakeRenderer {
 public:
  FakeRenderer() {}
  ~FakeRenderer() {}
  void ProcessFrame(scoped_refptr<VideoFrame> frame) {
    ReleaseOutputBuffer(frame.get());
  }
};

static int Run(bool use_dxva, const std::string& input_file) {
  // If we are not rendering, we need a window anyway to create a D3D device,
  // so we will just use the desktop window. (?)
  HWND window = GetDesktopWindow();
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
    dev_manager.Attach(CreateD3DDevManager(window,
                                           d3d9.Receive(),
                                           device.Receive()));
    if (dev_manager.get() == NULL) {
      LOG(ERROR) << "Cannot create D3D9 manager";
      return -1;
    }
  }
  scoped_ptr<MftH264Decoder> mft(new MftH264Decoder(use_dxva));
  scoped_ptr<FakeRenderer> renderer(new FakeRenderer());

  if (mft.get() == NULL || renderer.get() == NULL) {
    LOG(ERROR) << "Failed to create fake renderer / MFT";
    return -1;
  }
  if (!mft->Init(dev_manager,
                 frame_rate_num, frame_rate_denom,
                 width, height,
                 aspect_ratio_num, aspect_ratio_denom,
                 NewCallback(reader.get(), &FFmpegFileReader::Read2),
                 NewCallback(renderer.get(), &FakeRenderer::ProcessFrame))) {
    LOG(ERROR) << "Failed to initialize mft";
    return -1;
  }
  Time decode_start(Time::Now());
  while (true) {
    if (MftH264Decoder::kOutputOk != mft->GetOutput())
      break;
  }
  TimeDelta decode_time = Time::Now() - decode_start;

  printf("All done, frames read: %d, frames decoded: %d\n",
         mft->frames_read(), mft->frames_decoded());
  printf("Took %lldms\n", decode_time.InMilliseconds());
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
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
  std::string input_file = cmd_line.GetSwitchValueASCII("input-file");
  if (input_file.empty()) {
    fprintf(stderr, "No input file provided\n");
    usage();
    return -1;
  }
  printf("enable-dxva: %d\n", use_dxva);
  printf("input-file: %s\n", input_file.c_str());

  if (!InitFFmpeg()) {
    LOG(ERROR) << "InitFFMpeg() failed";
    return -1;
  }
  if (!InitComLibraries()) {
    LOG(ERROR) << "InitComLibraries() failed";
    return -1;
  }
  int ret = Run(use_dxva, input_file);
  ShutdownComLibraries();
  printf("Done\n");
  return ret;
}
