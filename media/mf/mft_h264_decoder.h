// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MFT H.264 decoder.

#ifndef MEDIA_MF_MFT_H264_DECODER_H_
#define MEDIA_MF_MFT_H264_DECODER_H_

#include "build/build_config.h"  // For OS_WIN.

#if defined(OS_WIN)

#include <deque>

#include <d3d9.h>
#include <dxva2api.h>
#include <mfidl.h>

#include "base/gtest_prod_util.h"
#include "base/scoped_comptr_win.h"
#include "media/video/video_decode_engine.h"

class MessageLoop;

namespace media {

class MftH264Decoder : public media::VideoDecodeEngine {
 public:
  typedef enum {
    kUninitialized,   // un-initialized.
    kNormal,          // normal playing state.
    kFlushing,        // upon received Flush(), before FlushDone()
    kEosDrain,        // upon input EOS received.
    kStopped,         // upon output EOS received.
  } State;

  explicit MftH264Decoder(bool use_dxva);
  ~MftH264Decoder();
  virtual void Initialize(MessageLoop* message_loop,
                          media::VideoDecodeEngine::EventHandler* event_handler,
                          const VideoCodecConfig& config);
  virtual void Uninitialize();
  virtual void Flush();
  virtual void Seek();
  virtual void EmptyThisBuffer(scoped_refptr<Buffer> buffer);
  virtual void FillThisBuffer(scoped_refptr<VideoFrame> frame);

  bool use_dxva() const { return use_dxva_; }
  State state() const { return state_; }

 private:
  friend class MftH264DecoderTest;
  FRIEND_TEST_ALL_PREFIXES(MftH264DecoderTest, LibraryInit);

  // TODO(jiesun): Find a way to move all these to GpuVideoService..
  static bool StartupComLibraries();
  static void ShutdownComLibraries();
  bool CreateD3DDevManager();

  bool InitInternal();
  bool InitDecoder();
  bool CheckDecoderDxvaSupport();
  bool SetDecoderMediaTypes();
  bool SetDecoderInputMediaType();
  bool SetDecoderOutputMediaType(const GUID subtype);
  bool SendMFTMessage(MFT_MESSAGE_TYPE msg);
  bool GetStreamsInfoAndBufferReqs();

  bool DoDecode();


  bool use_dxva_;

  ScopedComPtr<IDirect3D9> d3d9_;
  ScopedComPtr<IDirect3DDevice9> device_;
  ScopedComPtr<IDirect3DDeviceManager9> device_manager_;
  HWND device_window_;
  ScopedComPtr<IMFTransform> decoder_;

  MFT_INPUT_STREAM_INFO input_stream_info_;
  MFT_OUTPUT_STREAM_INFO output_stream_info_;

  State state_;

  VideoDecodeEngine::EventHandler* event_handler_;
  VideoCodecConfig config_;
  VideoCodecInfo info_;

  DISALLOW_COPY_AND_ASSIGN(MftH264Decoder);
};

}  // namespace media

#endif  // defined(OS_WIN)

#endif  // MEDIA_MF_MFT_H264_DECODER_H_
