// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GPU_VIDEO_DECODER_HOST_H_
#define CHROME_RENDERER_GPU_VIDEO_DECODER_HOST_H_

#include <deque>

#include "base/singleton.h"
#include "chrome/common/gpu_video_common.h"
#include "chrome/renderer/gpu_channel_host.h"
#include "ipc/ipc_message.h"
#include "media/base/buffers.h"
#include "media/base/video_frame.h"
#include "media/video/video_decode_engine.h"

using media::VideoFrame;
using media::Buffer;

class GpuVideoServiceHost;

// This class is used to talk to GpuVideoDecoder in the GPU process through
// IPC messages. It implements the interface of VideoDecodeEngine so users
// view it as a regular video decode engine, the implementation is a portal
// to the GPU process.
//
// THREAD SEMANTICS
//
// All methods of this class can be accessed on any thread. A message loop
// needs to be provided to class through Initialize() for accessing the
// IPC channel. Event handlers are called on that message loop.
//
// Since this class is not refcounted, it is important to delete this
// object only after OnUninitializeCompelte() is called.
class GpuVideoDecoderHost : public media::VideoDecodeEngine,
                            public IPC::Channel::Listener {
 public:
  virtual ~GpuVideoDecoderHost() {}

  // IPC::Channel::Listener.
  virtual void OnChannelConnected(int32 peer_pid) {}
  virtual void OnChannelError();
  virtual void OnMessageReceived(const IPC::Message& message);

  // media::VideoDecodeEngine implementation.
  virtual void Initialize(MessageLoop* message_loop,
                          VideoDecodeEngine::EventHandler* event_handler,
                          media::VideoDecodeContext* context,
                          const media::VideoCodecConfig& config);
  virtual void ConsumeVideoSample(scoped_refptr<Buffer> buffer);
  virtual void ProduceVideoFrame(scoped_refptr<VideoFrame> frame);
  virtual void Uninitialize();
  virtual void Flush();
  virtual void Seek();

 private:
  friend class GpuVideoServiceHost;

  // Internal states.
  enum GpuVideoDecoderHostState {
    kStateUninitialized,
    kStateNormal,
    kStateError,
    kStateFlushing,
  };

  // Private constructor.
  GpuVideoDecoderHost(GpuVideoServiceHost* service_host,
                      IPC::Message::Sender* ipc_sender,
                      int context_route_id);

  // Input message handler.
  void OnInitializeDone(const GpuVideoDecoderInitDoneParam& param);
  void OnUninitializeDone();
  void OnFlushDone();
  void OnEmptyThisBufferDone();
  void OnConsumeVideoFrame(int32 frame_id, int64 timestamp,
                           int64 duration, int32 flags);
  void OnEmptyThisBufferACK();

  // Helper function.
  void SendInputBufferToGpu();

  // Getter methods for IDs.
  int32 decoder_id() { return decoder_info_.decoder_id; }
  int32 route_id() { return decoder_info_.decoder_route_id; }
  int32 my_route_id() { return decoder_info_.decoder_host_route_id; }

  // We expect that GpuVideoServiceHost's always available during our life span.
  GpuVideoServiceHost* gpu_video_service_host_;

  // Sends IPC messages to the GPU process.
  IPC::Message::Sender* ipc_sender_;

  // Route ID of the GLES2 context in the GPU process.
  int context_route_id_;

  // We expect that the client of us will always available during our life span.
  EventHandler* event_handler_;

  // Globally identify this decoder in the GPU process.
  GpuVideoDecoderInfoParam decoder_info_;

  // Input buffer id serial number generator.
  int32 buffer_id_serial_;

  // Hold information about GpuVideoDecoder configuration.
  media::VideoCodecConfig config_;

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
