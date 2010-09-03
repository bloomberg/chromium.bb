// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_VIDEO_DECODER_H_
#define CHROME_GPU_GPU_VIDEO_DECODER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/gpu_video_common.h"
#include "media/video/video_decode_engine.h"
#include "ipc/ipc_channel.h"

using media::VideoCodecConfig;
using media::VideoCodecInfo;
using media::VideoStreamInfo;
using media::VideoFrame;
using media::Buffer;

namespace gpu {
namespace gles2 {
class GLES2Decoder;
}  // namespace gles2
}  // namespace gpu

class GpuChannel;

class GpuVideoDecoder
    : public IPC::Channel::Listener,
      public base::RefCountedThreadSafe<GpuVideoDecoder>,
      public media::VideoDecodeEngine::EventHandler {

 public:
  // IPC::Channel::Listener.
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();
  virtual void OnMessageReceived(const IPC::Message& message);

  // VideoDecodeEngine::EventHandler.
  virtual void OnInitializeComplete(const VideoCodecInfo& info);
  virtual void OnUninitializeComplete();
  virtual void OnFlushComplete();
  virtual void OnSeekComplete();
  virtual void OnError();
  virtual void OnFormatChange(VideoStreamInfo stream_info);
  virtual void OnEmptyBufferCallback(scoped_refptr<Buffer> buffer);
  virtual void OnFillBufferCallback(scoped_refptr<VideoFrame> frame);

  GpuVideoDecoder(const GpuVideoDecoderInfoParam* param,
                  GpuChannel* channel_,
                  base::ProcessHandle handle,
                  gpu::gles2::GLES2Decoder* decoder);
  virtual ~GpuVideoDecoder() {}

 private:
  int32 route_id() { return decoder_host_route_id_; }

  bool CreateInputTransferBuffer(uint32 size,
                                 base::SharedMemoryHandle* handle);
  bool CreateOutputTransferBuffer(uint32 size,
                                  base::SharedMemoryHandle* handle);
  void CreateVideoFrameOnTransferBuffer();

  int32 decoder_host_route_id_;

  // Used only in system memory path. i.e. Remove this later.
  scoped_refptr<VideoFrame> frame_;
  bool output_transfer_buffer_busy_;
  int32 pending_output_requests_;

  GpuChannel* channel_;
  base::ProcessHandle renderer_handle_;

  // The GLES2 decoder has the context associated with this decoder. This object
  // is used to switch GLES2 context and translate client texture ID to service
  // ID.
  gpu::gles2::GLES2Decoder* gles2_decoder_;

  scoped_ptr<base::SharedMemory> input_transfer_buffer_;
  scoped_ptr<base::SharedMemory> output_transfer_buffer_;

  scoped_ptr<media::VideoDecodeEngine> decode_engine_;
  media::VideoCodecConfig config_;
  media::VideoCodecInfo info_;

  // Input message handler.
  void OnInitialize(const GpuVideoDecoderInitParam& param);
  void OnUninitialize();
  void OnFlush();
  void OnEmptyThisBuffer(const GpuVideoDecoderInputBufferParam& buffer);
  void OnFillThisBuffer(const GpuVideoDecoderOutputBufferParam& frame);
  void OnFillThisBufferDoneACK();

  // Output message helper.
  void SendInitializeDone(const GpuVideoDecoderInitDoneParam& param);
  void SendUninitializeDone();
  void SendFlushDone();
  void SendEmptyBufferDone();
  void SendEmptyBufferACK();
  void SendFillBufferDone(const GpuVideoDecoderOutputBufferParam& frame);

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecoder);
};

#endif  // CHROME_GPU_GPU_VIDEO_DECODER_H_
