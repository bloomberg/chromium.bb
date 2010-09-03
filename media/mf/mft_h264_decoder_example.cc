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
#include "media/base/data_buffer.h"
#include "media/base/media.h"
#include "media/base/video_frame.h"
#include "media/base/yuv_convert.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/file_protocol.h"
#include "media/mf/file_reader_util.h"
#include "media/mf/mft_h264_decoder.h"

using base::AtExitManager;
using base::Time;
using base::TimeDelta;
using media::Buffer;
using media::DataBuffer;
using media::FFmpegFileReader;
using media::MftH264Decoder;
using media::VideoCodecConfig;
using media::VideoCodecInfo;
using media::VideoDecodeEngine;
using media::VideoFrame;
using media::VideoStreamInfo;

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
  RECT rect;
  rect.left = 0;
  rect.right = width;
  rect.top = 0;
  rect.bottom = height;
  AdjustWindowRect(&rect, kWindowStyleFlags, FALSE);
  MoveWindow(window, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
             TRUE);
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
      // Seek forward 5 seconds.
      decoder_->Flush();
      reader_->SeekForward(5000000);
    }
  }

  virtual void DidProcessMessage(const MSG& msg) {
  }

 private:
  FFmpegFileReader* reader_;
  MftH264Decoder* decoder_;
};

class MftH264DecoderHandler
    : public VideoDecodeEngine::EventHandler,
      public base::RefCountedThreadSafe<MftH264DecoderHandler> {
 public:
  MftH264DecoderHandler() : frames_read_(0), frames_decoded_(0) {
    memset(&info_, 0, sizeof(info_));
  }
  virtual ~MftH264DecoderHandler() {}
  virtual void OnInitializeComplete(const VideoCodecInfo& info) {
    info_ = info;
  }
  virtual void OnUninitializeComplete() {
  }
  virtual void OnFlushComplete() {
  }
  virtual void OnSeekComplete() {}
  virtual void OnError() {}
  virtual void OnFormatChange(VideoStreamInfo stream_info) {
    info_.stream_info_ = stream_info;
  }
  virtual void OnEmptyBufferCallback(scoped_refptr<Buffer> buffer) {
    if (reader_ && decoder_) {
      scoped_refptr<DataBuffer> input;
      reader_->Read(&input);
      if (!input->IsEndOfStream())
        frames_read_++;
      decoder_->EmptyThisBuffer(input);
    }
  }
  virtual void OnFillBufferCallback(scoped_refptr<VideoFrame> frame) {
    if (frame.get()) {
      if (frame->format() != VideoFrame::EMPTY) {
        frames_decoded_++;
      }
    }
  }
  virtual void SetReader(FFmpegFileReader* reader) {
    reader_ = reader;
  }
  virtual void SetDecoder(MftH264Decoder* decoder) {
    decoder_= decoder;
  }
  virtual void DecodeSingleFrame() {
    scoped_refptr<VideoFrame> frame;
    decoder_->FillThisBuffer(frame);
  }
  virtual void Start() {
    while (decoder_->state() != MftH264Decoder::kStopped)
      DecodeSingleFrame();
  }

  VideoCodecInfo info_;
  int frames_read_;
  int frames_decoded_;
  FFmpegFileReader* reader_;
  MftH264Decoder* decoder_;
};

class RenderToWindowHandler : public MftH264DecoderHandler {
 public:
  RenderToWindowHandler(HWND window, MessageLoop* loop)
      : MftH264DecoderHandler(),
        window_(window),
        loop_(loop),
        has_output_(false) {
  }
  virtual ~RenderToWindowHandler() {}
  virtual void OnFillBufferCallback(scoped_refptr<VideoFrame> frame) {
    has_output_ = true;
    if (frame.get()) {
      if (frame->format() != VideoFrame::EMPTY) {
        frames_decoded_++;
        loop_->PostDelayedTask(
            FROM_HERE,
            NewRunnableMethod(this, &RenderToWindowHandler::DecodeSingleFrame),
            frame->GetDuration().InMilliseconds());

        int width = frame->width();
        int height = frame->height();

        // Assume height does not change.
        static uint8* rgb_frame = new uint8[height * frame->stride(0) * 4];
        uint8* frame_y = static_cast<uint8*>(frame->data(VideoFrame::kYPlane));
        uint8* frame_u = static_cast<uint8*>(frame->data(VideoFrame::kUPlane));
        uint8* frame_v = static_cast<uint8*>(frame->data(VideoFrame::kVPlane));
        media::ConvertYUVToRGB32(frame_y, frame_v, frame_u, rgb_frame,
                                 width, height,
                                 frame->stride(0), frame->stride(1),
                                 4 * frame->stride(0), media::YV12);
        PAINTSTRUCT ps;
        InvalidateRect(window_, NULL, TRUE);
        HDC hdc = BeginPaint(window_, &ps);
        BITMAPINFOHEADER hdr;
        hdr.biSize = sizeof(BITMAPINFOHEADER);
        hdr.biWidth = width;
        hdr.biHeight = -height;      // minus means top-down bitmap
        hdr.biPlanes = 1;
        hdr.biBitCount = 32;
        hdr.biCompression = BI_RGB;  // no compression
        hdr.biSizeImage = 0;
        hdr.biXPelsPerMeter = 1;
        hdr.biYPelsPerMeter = 1;
        hdr.biClrUsed = 0;
        hdr.biClrImportant = 0;
        int rv = StretchDIBits(hdc, 0, 0, width, height, 0, 0, width, height,
                               rgb_frame, reinterpret_cast<BITMAPINFO*>(&hdr),
                               DIB_RGB_COLORS, SRCCOPY);
        EndPaint(window_, &ps);
        if (!rv) {
          LOG(ERROR) << "StretchDIBits failed";
          loop_->QuitNow();
        }
      } else {    // if frame is type EMPTY, there will be no more frames.
        loop_->QuitNow();
      }
    }
  }
  virtual void DecodeSingleFrame() {
    if (decoder_->state() != MftH264Decoder::kStopped) {
      while (decoder_->state() != MftH264Decoder::kStopped && !has_output_) {
        scoped_refptr<VideoFrame> frame;
        decoder_->FillThisBuffer(frame);
      }
      if (decoder_->state() == MftH264Decoder::kStopped)
        loop_->QuitNow();
      has_output_ = false;
    } else {
      loop_->QuitNow();
    }
  }
  virtual void Start() {
    loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RenderToWindowHandler::DecodeSingleFrame));
    loop_->Run();
  }

 private:
  HWND window_;
  MessageLoop* loop_;
  bool has_output_;
};

static int Run(bool use_dxva, bool render, const std::string& input_file) {
  scoped_ptr<FFmpegFileReader> reader(new FFmpegFileReader(input_file));
  if (reader.get() == NULL || !reader->Initialize()) {
    LOG(ERROR) << "Failed to create/initialize reader";
    return -1;
  }
  int width = 0, height = 0;
  if (!reader->GetWidth(&width) || !reader->GetHeight(&height)) {
    LOG(WARNING) << "Failed to get width/height from reader";
  }
  VideoCodecConfig config;
  config.width_ = width;
  config.height_ = height;
  HWND window = NULL;
  if (render) {
    window = CreateDrawWindow(width, height);
    if (window == NULL) {
      LOG(ERROR) << "Failed to create window";
      return -1;
    }
  }

  scoped_ptr<MftH264Decoder> mft(new MftH264Decoder(use_dxva));
  if (!mft.get()) {
    LOG(ERROR) << "Failed to create fake MFT";
    return -1;
  }

  scoped_refptr<MftH264DecoderHandler> handler;
  if (render)
    handler = new RenderToWindowHandler(window, MessageLoop::current());
  else
    handler = new MftH264DecoderHandler();
  handler->SetDecoder(mft.get());
  handler->SetReader(reader.get());
  if (!handler.get()) {
    LOG(ERROR) << "FAiled to create handler";
    return -1;
  }

  mft->Initialize(MessageLoop::current(), handler.get(), config);
  scoped_ptr<WindowObserver> observer;
  if (render) {
    observer.reset(new WindowObserver(reader.get(), mft.get()));
    MessageLoopForUI::current()->AddObserver(observer.get());
  }

  Time decode_start(Time::Now());
  handler->Start();
  TimeDelta decode_time = Time::Now() - decode_start;

  printf("All done, frames read: %d, frames decoded: %d\n",
         handler->frames_read_, handler->frames_decoded_);
  printf("Took %lldms\n", decode_time.InMilliseconds());
  if (window)
    DestroyWindow(window);
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
  int ret = Run(use_dxva, render, input_file);

  printf("Done\n");
  return ret;
}
