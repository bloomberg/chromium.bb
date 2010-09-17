// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GPU_VIDEO_DECODER_HOST_H_
#define CHROME_RENDERER_GPU_VIDEO_DECODER_HOST_H_

#include <deque>

#include "base/singleton.h"
#include "chrome/common/gpu_video_common.h"
#include "chrome/renderer/gpu_channel_host.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/base/buffers.h"
#include "media/base/video_frame.h"

using media::VideoFrame;
using media::Buffer;

class GpuVideoServiceHost;

class GpuVideoDecoderHost
    : public base::RefCountedThreadSafe<GpuVideoDecoderHost>,
      public IPC::Channel::Listener {
 public:
  class EventHandler {
   public:
    virtual void OnInitializeDone(
        bool success,
        const GpuVideoDecoderInitDoneParam& param) = 0;
    virtual void OnUninitializeDone() = 0;
    virtual void OnFlushDone() = 0;
    virtual void OnEmptyBufferDone(scoped_refptr<Buffer> buffer) = 0;
    virtual void OnFillBufferDone(scoped_refptr<VideoFrame> frame) = 0;
    virtual void OnDeviceError() = 0;
  };

  typedef enum {
    kStateUninitialized,
    kStateNormal,
    kStateError,
    kStateFlushing,
  } GpuVideoDecoderHostState;

  // IPC::Channel::Listener.
  virtual void OnChannelConnected(int32 peer_pid) {}
  virtual void OnChannelError();
  virtual void OnMessageReceived(const IPC::Message& message);

  bool Initialize(EventHandler* handler, const GpuVideoDecoderInitParam& param);
  bool Uninitialize();
  void EmptyThisBuffer(scoped_refptr<Buffer> buffer);
  void FillThisBuffer(scoped_refptr<VideoFrame> frame);
  bool Flush();

  int32 decoder_id() { return decoder_info_.decoder_id; }
  int32 route_id() { return decoder_info_.decoder_route_id; }
  int32 my_route_id() { return decoder_info_.decoder_host_route_id; }

  virtual ~GpuVideoDecoderHost() {}

 private:
  friend class GpuVideoServiceHost;
  GpuVideoDecoderHost(GpuVideoServiceHost* service_host,
                      GpuChannelHost* channel_host,
                      int context_route_id);

  // Input message handler.
  void OnInitializeDone(const GpuVideoDecoderInitDoneParam& param);
  void OnUninitializeDone();
  void OnFlushDone();
  void OnEmptyThisBufferDone();
  void OnFillThisBufferDone(const GpuVideoDecoderOutputBufferParam& param);
  void OnEmptyThisBufferACK();

  // Helper function.
  void SendInputBufferToGpu();

  // We expect that GpuVideoServiceHost's always available during our life span.
  GpuVideoServiceHost* gpu_video_service_host_;

  GpuChannelHost* channel_host_;

  // Route ID of the GLES2 context in the GPU process.
  int context_route_id_;

  // We expect that the client of us will always available during our life span.
  EventHandler* event_handler_;

  // Globally identify this decoder in the GPU process.
  GpuVideoDecoderInfoParam decoder_info_;

  // Input buffer id serial number generator.
  int32 buffer_id_serial_;

  // Hold information about GpuVideoDecoder configuration.
  GpuVideoDecoderInitParam init_param_;

  // Hold information about output surface format, etc.
  GpuVideoDecoderInitDoneParam done_param_;

  // Current state of video decoder.
  GpuVideoDecoderHostState state_;

  // We are not able to push all received buffer to gpu process at once.
  std::deque<scoped_refptr<Buffer> > input_buffer_queue_;

  // Currently we do not use ring buffer in input buffer, therefore before
  // GPU process had finished access it, we should not touch it.
  bool input_buffer_busy_;

  // Transfer buffers for both input and output.
  // TODO(jiesun): remove output buffer when hardware composition is ready.
  scoped_ptr<base::SharedMemory> input_transfer_buffer_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecoderHost);
};

#endif  // CHROME_RENDERER_GPU_VIDEO_DECODER_HOST_H_
