// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// Demonstrates the use of H264Mft.

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
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/file_protocol.h"
#include "media/mf/file_reader_util.h"
#include "media/mf/h264mft.h"

using media::FFmpegFileReader;
using media::H264Mft;
using media::VideoFrame;

namespace {

void usage() {
  static char* usage_msg =
      "Usage: h264mft [--enable-dxva] --input-file=FILE\n"
      "enable-dxva: Enables hardware accelerated decoding\n"
      "To display this message: h264mft --help";
  fprintf(stderr, "%s\n", usage_msg);
}

static bool InitFFmpeg() {
  if (!media::InitializeMediaLibrary(FilePath()))
    return false;
  avcodec_init();
  av_register_all();
  av_register_protocol(&kFFmpegFileProtocol);
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

IDirect3DDeviceManager9* CreateD3DDevManager(HWND video_window,
                                             int width,
                                             int height,
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

  present_params.BackBufferWidth = width;
  present_params.BackBufferHeight = height;
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
                                 video_window,
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

// Example usage of how to get a decoded frame from the decoder.
bool GetDecodedSample(FFmpegFileReader* reader, H264Mft* decoder,
                      scoped_refptr<VideoFrame>* decoded_frame) {
  // Keep feeding the MFT with inputs until it spits out an output.
  for (;;) {
    // First check if there is output.
    H264Mft::DecoderOutputState state = decoder->GetOutput(decoded_frame);
    if (state == H264Mft::kOutputOk) {
      LOG(INFO) << "Got an output from decoder";
      return true;
    } else if (state == H264Mft::kResetOutputStreamFailed) {
      LOG(ERROR) << "Reset output stream failed, quitting";
      return false;
    } else if (state == H264Mft::kResetOutputStreamOk) {
      LOG(INFO) << "Reset output stream, try to get output again";
      continue;
    } else if (state == H264Mft::kNeedMoreInput) {
      LOG(INFO) << "Need more input";
      uint8* input_stream_dummy;
      int size;
      int duration;
      int64 timestamp;
      reader->Read(&input_stream_dummy, &size, &duration, &timestamp);
      scoped_array<uint8> input_stream(input_stream_dummy);
      if (input_stream.get() == NULL) {
        LOG(INFO) << "No more input, sending drain message to decoder";
        if (!decoder->SendDrainMessage()) {
          LOG(ERROR) << "Failed to send drain message, quitting";
          return false;
        } else {
          continue;   // Try reading the rest of the drained outputs.
        }
      } else {
        // We read an input stream, we can feed it into the decoder.
        if (!decoder->SendInput(input_stream.get(), size,
            reader->ConvertFFmpegTimeBaseTo100Ns(timestamp),
            reader->ConvertFFmpegTimeBaseTo100Ns(duration))) {
          LOG(ERROR) << "Failed to send input, dropping frame...";
        }
        continue;  // Try reading the output after attempting to send an input.
      }
    } else if (state == H264Mft::kNoMoreOutput) {
      LOG(INFO) << "Decoder has processed everything, quitting";
      return false;
    } else if (state == H264Mft::kUnspecifiedError) {
      LOG(ERROR) << "Unknown error, quitting";
      return false;
    } else if (state == H264Mft::kNoMemory) {
      LOG(ERROR) << "Not enough memory for sample, quitting";
      return false;
    } else if (state == H264Mft::kOutputSampleError) {
      LOG(ERROR) << "Inconsistent sample, dropping...";
      continue;
    } else {
      NOTREACHED();
    }
  }  // for (;;)
  NOTREACHED();
}

static void ReleaseOutputBuffer(VideoFrame* frame) {
  if (frame->type() == VideoFrame::TYPE_MFBUFFER ||
      frame->type() == VideoFrame::TYPE_DIRECT3DSURFACE) {
    static_cast<IMFMediaBuffer*>(frame->private_buffer())->Release();
  } else {
    return;
  }
}

int Run(bool use_dxva, const std::string& input_file) {
  scoped_ptr<FFmpegFileReader> reader(new FFmpegFileReader(input_file));
  if (reader.get() == NULL) {
    LOG(ERROR) << "Failed to create reader";
    return -1;
  }
  if (!reader->Initialize()) {
    LOG(ERROR) << "Failed to initialize reader";
    return -1;
  }
  int frame_rate_num = 0, frame_rate_denom = 0;
  if (!reader->GetFrameRate(&frame_rate_num, &frame_rate_denom)) {
    LOG(WARNING) << "Failed to get frame rate from reader";
  }
  int width = 0, height = 0;
  if (!reader->GetWidth(&width) || !reader->GetHeight(&height)) {
    LOG(WARNING) << "Failed to get width/height from reader";
  }
  int aspect_ratio_num = 0, aspect_ratio_denom = 0;
  if (!reader->GetAspectRatio(&aspect_ratio_num, &aspect_ratio_denom)) {
    LOG(WARNING) << "Failed to get aspect ratio from reader";
  }
  ScopedComPtr<IDirect3D9> d3d9;
  ScopedComPtr<IDirect3DDevice9> device;
  ScopedComPtr<IDirect3DDeviceManager9> dev_manager;
  if (use_dxva) {
    dev_manager.Attach(CreateD3DDevManager(GetDesktopWindow(),
                                           width,
                                           height,
                                           d3d9.Receive(),
                                           device.Receive()));
    if (dev_manager.get() == NULL) {
      LOG(ERROR) << "Cannot create D3D9 manager";
      return -1;
    }
  }
  scoped_ptr<H264Mft> mft(new H264Mft(use_dxva));
  if (mft.get() == NULL) {
    LOG(ERROR) << "Failed to create MFT";
    return -1;
  }
  if (!mft->Init(dev_manager, frame_rate_num, frame_rate_denom, width, height,
                 aspect_ratio_num, aspect_ratio_denom)) {
    LOG(ERROR) << "Failed to initialize mft";
    return -1;
  }
  base::TimeDelta decode_time;
  while (true) {
    // Do nothing with the sample except to let it go out of scope
    scoped_refptr<VideoFrame> decoded_frame;
    base::Time decode_start(base::Time::Now());
    if (!GetDecodedSample(reader.get(), mft.get(), &decoded_frame))
      break;
    decode_time += base::Time::Now() - decode_start;
    ReleaseOutputBuffer(decoded_frame.get());
  }
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
