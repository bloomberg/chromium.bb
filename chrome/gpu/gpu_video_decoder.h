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
#include "chrome/gpu/media/gpu_video_device.h"
#include "media/video/video_decode_context.h"
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

// A GpuVideoDecoder is a platform independent video decoder that uses platform
// specific VideoDecodeEngine and GpuVideoDevice for the actual decoding
// operations.
//
// In addition to delegating video related commamnds to VideoDecodeEngine it
// has the following important functions:
//
// Buffer Allocation
//
// VideoDecodeEngine requires platform specific video frame buffer to operate.
// In order to abstract the platform specific bits GpuVideoDecoderContext is
// used to allocate video frames that a VideoDecodeEngine can use.
//
// Since all the video frames appear to the Renderer process as textures, the
// first thing GpuVideoDecoder needs to do is to ask Renderer process to
// generate and allocate textures. This will establish a texture record in the
// command buffer decoder. After the texture is allocated, this class will
// pass the textures meaningful in the local GLES context to
// GpuVideoDevice for generating VideoFrames that VideoDecodeEngine
// can actually use.
//
// In order to coordinate these operations, GpuVideoDecoder implements
// VideoDecodeContext and is injected into the VideoDecodeEngine.
//
// The sequence of operations is:
// 1. VideoDecodeEngine requests by call AllocateVideoFrames().
// 2. GpuVideoDecoder receives AllocateVideoFrames() and then call to the
//    Renderer process to generate textures.
// 3. Renderer process replied with textures.
// 4. Textures generated are passed into GpuVideoDevice.
// 5. GpuVideoDevice::AllocateVideoFrames() is called to generate
//    VideoFrame(s) from the textures.
// 6. GpuVideoDecoder sends the VideoFrame(s) generated to VideoDecodeEngine.
//
// Buffer Translation
//
// GpuVideoDecoder will be working with VideoDecodeEngine, they exchange
// buffers that are only meaningful to VideoDecodeEngine. In order to translate
// that to something we can transport in the IPC channel we need a mapping
// between VideoFrame and buffer ID known between GpuVideoDecoder and
// GpuVideoDecoderHost in the Renderer process.
//
// After texture allocation and VideoFrame allocation are done, GpuVideoDecoder
// will maintain such mapping.
//
class GpuVideoDecoder
    : public IPC::Channel::Listener,
      public base::RefCountedThreadSafe<GpuVideoDecoder>,
      public media::VideoDecodeEngine::EventHandler,
      public media::VideoDecodeContext {

 public:
  // IPC::Channel::Listener implementation.
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();
  virtual void OnMessageReceived(const IPC::Message& message);

  // VideoDecodeEngine::EventHandler implementation.
  virtual void OnInitializeComplete(const VideoCodecInfo& info);
  virtual void OnUninitializeComplete();
  virtual void OnFlushComplete();
  virtual void OnSeekComplete();
  virtual void OnError();
  virtual void OnFormatChange(VideoStreamInfo stream_info);
  virtual void ProduceVideoSample(scoped_refptr<Buffer> buffer);
  virtual void ConsumeVideoFrame(scoped_refptr<VideoFrame> frame);

  // VideoDecodeContext implementation.
  virtual void* GetDevice();
  virtual void AllocateVideoFrames(int n, size_t width, size_t height,
                                   AllocationCompleteCallback* callback);
  virtual void ReleaseVideoFrames(int n, VideoFrame* frames);
  virtual void Destroy(DestructionCompleteCallback* callback);

  // Constructor and destructor.
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
  // is used to switch context and translate client texture ID to service ID.
  gpu::gles2::GLES2Decoder* gles2_decoder_;

  scoped_ptr<base::SharedMemory> input_transfer_buffer_;
  scoped_ptr<base::SharedMemory> output_transfer_buffer_;

  scoped_ptr<media::VideoDecodeEngine> decode_engine_;
  scoped_ptr<GpuVideoDevice> decode_context_;
  media::VideoCodecConfig config_;
  media::VideoCodecInfo info_;

  // Input message handler.
  void OnInitialize(const GpuVideoDecoderInitParam& param);
  void OnUninitialize();
  void OnFlush();
  void OnEmptyThisBuffer(const GpuVideoDecoderInputBufferParam& buffer);
  void OnFillThisBuffer(const GpuVideoDecoderOutputBufferParam& param);
  void OnFillThisBufferDoneACK();

  // Output message helper.
  void SendInitializeDone(const GpuVideoDecoderInitDoneParam& param);
  void SendUninitializeDone();
  void SendFlushDone();
  void SendEmptyBufferDone();
  void SendEmptyBufferACK();
  void SendFillBufferDone(const GpuVideoDecoderOutputBufferParam& param);

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecoder);
};

#endif  // CHROME_GPU_GPU_VIDEO_DECODER_H_
