// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_VIDEO_DECODER_H_
#define CHROME_GPU_GPU_VIDEO_DECODER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/gpu_video_common.h"
#include "chrome/gpu/media/gpu_video_device.h"
#include "media/video/video_decode_context.h"
#include "media/video/video_decode_engine.h"
#include "ipc/ipc_channel.h"

using media::Buffer;
using media::PipelineStatistics;
using media::VideoCodecConfig;
using media::VideoCodecInfo;
using media::VideoStreamInfo;
using media::VideoFrame;

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
// BUFFER ALLOCATION
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
// BUFFER UPLOADING
//
// A VideoDecodeEngine always produces some device specific buffer. In order to
// use them in Chrome we always upload them to GL textures. The upload step is
// different on each platform and each subsystem. We perform these special
// upload steps by using GpuVideoDevice which are written for each
// VideoDecodeEngine.
//
// BUFFER MAPPING
//
// GpuVideoDecoder will be working with VideoDecodeEngine, they exchange
// buffers that are only meaningful to VideoDecodeEngine. In order to map that
// to something we can transport in the IPC channel we need a mapping between
// VideoFrame and buffer ID known between GpuVideoDecoder and
// GpuVideoDecoderHost in the Renderer process.
//
// After texture allocation and VideoFrame allocation are done, GpuVideoDecoder
// will maintain such mapping.
//
class GpuVideoDecoder
    : public base::RefCountedThreadSafe<GpuVideoDecoder>,
      public IPC::Channel::Listener,
      public media::VideoDecodeEngine::EventHandler,
      public media::VideoDecodeContext {
 public:
  // Constructor and destructor.
  GpuVideoDecoder(MessageLoop* message_loop,
                  int32 decoder_host_id,
                  IPC::Message::Sender* sender,
                  base::ProcessHandle handle,
                  gpu::gles2::GLES2Decoder* decoder);
  virtual ~GpuVideoDecoder();

  // IPC::Channel::Listener implementation.
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();
  virtual bool OnMessageReceived(const IPC::Message& message);

  // VideoDecodeEngine::EventHandler implementation.
  virtual void OnInitializeComplete(const VideoCodecInfo& info);
  virtual void OnUninitializeComplete();
  virtual void OnFlushComplete();
  virtual void OnSeekComplete();
  virtual void OnError();
  virtual void OnFormatChange(VideoStreamInfo stream_info);
  virtual void ProduceVideoSample(scoped_refptr<Buffer> buffer);
  virtual void ConsumeVideoFrame(scoped_refptr<VideoFrame> frame,
                                 const PipelineStatistics& statistics);

  // VideoDecodeContext implementation.
  virtual void* GetDevice();
  virtual void AllocateVideoFrames(
      int n, size_t width, size_t height, media::VideoFrame::Format format,
      std::vector<scoped_refptr<media::VideoFrame> >* frames, Task* task);
  virtual void ReleaseAllVideoFrames();
  virtual void ConvertToVideoFrame(void* buffer,
                                   scoped_refptr<media::VideoFrame> frame,
                                   Task* task);
  virtual void Destroy(Task* task);

  // These methods are used in unit test only.
  void SetVideoDecodeEngine(media::VideoDecodeEngine* engine);
  void SetGpuVideoDevice(GpuVideoDevice* device);

 private:
  struct PendingAllocation;

  int32 decoder_host_id() { return decoder_host_id_; }

  bool CreateInputTransferBuffer(uint32 size,
                                 base::SharedMemoryHandle* handle);

  // These methods are message handlers for the messages sent from the Renderer
  // process.
  void OnInitialize(const GpuVideoDecoderInitParam& param);
  void OnUninitialize();
  void OnFlush();
  void OnPreroll();
  void OnEmptyThisBuffer(const GpuVideoDecoderInputBufferParam& buffer);
  void OnProduceVideoFrame(int32 frame_id);
  void OnVideoFrameAllocated(int32 frame_id, std::vector<uint32> textures);

  // Helper methods for sending messages to the Renderer process.
  void SendInitializeDone(const GpuVideoDecoderInitDoneParam& param);
  void SendUninitializeDone();
  void SendFlushDone();
  void SendPrerollDone();
  void SendEmptyBufferDone();
  void SendEmptyBufferACK();
  void SendConsumeVideoFrame(int32 frame_id, int64 timestamp, int64 duration,
                             int32 flags);
  void SendAllocateVideoFrames(
      int n, size_t width, size_t height, media::VideoFrame::Format format);
  void SendReleaseAllVideoFrames();

  // The message loop that this object should run on.
  MessageLoop* message_loop_;

  // ID of GpuVideoDecoderHost in the Renderer Process.
  int32 decoder_host_id_;

  // Used only in system memory path. i.e. Remove this later.
  scoped_refptr<VideoFrame> frame_;

  IPC::Message::Sender* sender_;
  base::ProcessHandle renderer_handle_;

  // The GLES2 decoder has the context associated with this decoder. This object
  // is used to switch context and translate client texture ID to service ID.
  gpu::gles2::GLES2Decoder* gles2_decoder_;

  // Memory for transfering the input data for the hardware video decoder.
  scoped_ptr<base::SharedMemory> input_transfer_buffer_;

  // VideoDecodeEngine is used to do the actual video decoding.
  scoped_ptr<media::VideoDecodeEngine> decode_engine_;

  // GpuVideoDevice is used to generate VideoFrame(s) from GL textures. The
  // frames generated are understood by the decode engine.
  scoped_ptr<GpuVideoDevice> video_device_;

  // Contain information for allocation VideoFrame(s).
  scoped_ptr<PendingAllocation> pending_allocation_;

  // Contains the mapping between a |frame_id| and VideoFrame generated by
  // GpuVideoDevice from the associated GL textures.
  // TODO(hclam): Using a faster data structure than map.
  typedef std::map<int32, scoped_refptr<media::VideoFrame> > VideoFrameMap;
  VideoFrameMap video_frame_map_;

  media::VideoCodecConfig config_;
  media::VideoCodecInfo info_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecoder);
};

#endif  // CHROME_GPU_GPU_VIDEO_DECODER_H_
