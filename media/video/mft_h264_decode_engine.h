// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MFT H.264 decode engine.

#ifndef MEDIA_VIDEO_MFT_H264_DECODE_ENGINE_H_
#define MEDIA_VIDEO_MFT_H264_DECODE_ENGINE_H_

// TODO(imcheng): Get rid of this header by:
// - forward declaring IMFTransform and its IID as in
//   mft_h264_decode_engine_context.h
// - turning the general SendMFTMessage method into specific methods
//   (SendFlushMessage, SendDrainMessage, etc.) to avoid having
//   MFT_MESSAGE_TYPE in here
#include <mfidl.h>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/scoped_comptr_win.h"
#include "media/video/video_decode_engine.h"

struct IDirect3DSurface9;
extern "C" const GUID IID_IDirect3DSurface9;

class MessageLoop;

namespace media {

class VideoDecodeContext;

class MftH264DecodeEngine : public media::VideoDecodeEngine {
 public:
  typedef enum {
    kUninitialized,   // un-initialized.
    kNormal,          // normal playing state.
    kFlushing,        // upon received Flush(), before FlushDone()
    kEosDrain,        // upon input EOS received.
    kStopped,         // upon output EOS received.
  } State;

  explicit MftH264DecodeEngine(bool use_dxva);
  virtual ~MftH264DecodeEngine();

  // VideoDecodeEngine implementation.
  virtual void Initialize(MessageLoop* message_loop,
                          media::VideoDecodeEngine::EventHandler* event_handler,
                          VideoDecodeContext* context,
                          const VideoCodecConfig& config);
  virtual void Uninitialize();
  virtual void Flush();
  virtual void Seek();
  virtual void ConsumeVideoSample(scoped_refptr<Buffer> buffer);
  virtual void ProduceVideoFrame(scoped_refptr<VideoFrame> frame);

  bool use_dxva() const { return use_dxva_; }
  State state() const { return state_; }

 private:
  friend class MftH264DecodeEngineTest;
  FRIEND_TEST_ALL_PREFIXES(MftH264DecodeEngineTest, LibraryInit);

  // TODO(jiesun): Find a way to move all these to GpuVideoService..
  static bool StartupComLibraries();
  static void ShutdownComLibraries();
  bool EnableDxva();

  bool InitInternal();
  bool InitDecodeEngine();
  void AllocFramesFromContext();
  bool CheckDecodeEngineDxvaSupport();
  bool SetDecodeEngineMediaTypes();
  bool SetDecodeEngineInputMediaType();
  bool SetDecodeEngineOutputMediaType(const GUID subtype);
  bool SendMFTMessage(MFT_MESSAGE_TYPE msg);
  bool GetStreamsInfoAndBufferReqs();
  bool DoDecode(const PipelineStatistics& statistics);
  void OnAllocFramesDone();
  void OnUploadVideoFrameDone(
      ScopedComPtr<IDirect3DSurface9, &IID_IDirect3DSurface9> surface,
      scoped_refptr<media::VideoFrame> frame, PipelineStatistics statistics);

  bool use_dxva_;
  ScopedComPtr<IMFTransform> decode_engine_;

  MFT_INPUT_STREAM_INFO input_stream_info_;
  MFT_OUTPUT_STREAM_INFO output_stream_info_;

  State state_;

  int width_;
  int height_;

  VideoDecodeEngine::EventHandler* event_handler_;
  VideoCodecInfo info_;

  VideoDecodeContext* context_;
  std::vector<scoped_refptr<VideoFrame> > output_frames_;

  DISALLOW_COPY_AND_ASSIGN(MftH264DecodeEngine);
};

}  // namespace media

#endif  // MEDIA_VIDEO_MFT_H264_DECODE_ENGINE_H_
