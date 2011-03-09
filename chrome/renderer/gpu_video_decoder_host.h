// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GPU_VIDEO_DECODER_HOST_H_
#define CHROME_RENDERER_GPU_VIDEO_DECODER_HOST_H_

#include <deque>
#include <map>

#include "base/singleton.h"
#include "chrome/common/gpu_video_common.h"
#include "chrome/renderer/gpu_channel_host.h"
#include "ipc/ipc_message.h"
#include "media/base/buffers.h"
#include "media/base/video_frame.h"
#include "media/video/video_decode_engine.h"

using media::VideoFrame;
using media::Buffer;

class MessageRouter;

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
  // |router| is used to dispatch IPC messages to this object.
  // |ipc_sender| is used to send IPC messages to GPU process.
  // It is important that the above two objects are accessed on the
  // |message_loop_|.
  GpuVideoDecoderHost(MessageRouter* router,
                      IPC::Message::Sender* ipc_sender,
                      int context_route_id,
                      int32 decoder_host_id);
  virtual ~GpuVideoDecoderHost();

  // IPC::Channel::Listener.
  virtual void OnChannelConnected(int32 peer_pid) {}
  virtual void OnChannelError();
  virtual bool OnMessageReceived(const IPC::Message& message);

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
  typedef std::map<int32, scoped_refptr<media::VideoFrame> > VideoFrameMap;

  // Internal states.
  enum GpuVideoDecoderHostState {
    kStateUninitialized,
    kStateNormal,
    kStateError,
    kStateFlushing,
  };

  // Takes care of sending IPC message to create a video decoder.
  void CreateVideoDecoder();

  // Handlers for messages received from the GPU process.
  void OnCreateVideoDecoderDone(int32 decoder_id);
  void OnInitializeDone(const GpuVideoDecoderInitDoneParam& param);
  void OnUninitializeDone();
  void OnFlushDone();
  void OnPrerollDone();
  void OnEmptyThisBufferACK();
  void OnProduceVideoSample();
  void OnConsumeVideoFrame(int32 frame_id, int64 timestamp,
                           int64 duration, int32 flags);
  void OnAllocateVideoFrames(int32 n, uint32 width,
                             uint32 height, int32 format);
  void OnReleaseAllVideoFrames();

  // Handler for VideoDecodeContext. This method is called when video frames
  // allocation is done.
  void OnAllocateVideoFramesDone();

  // Send a message to the GPU process to inform that a video frame is
  // allocated.
  void SendVideoFrameAllocated(int32 frame_id,
                               scoped_refptr<media::VideoFrame> frame);

  // Send a video sample to the GPU process and tell it to use the buffer for
  // video decoding.
  void SendConsumeVideoSample();

  // Look up the frame_id for |frame| and send a message to the GPU process
  // to use that video frame to produce an output.
  void SendProduceVideoFrame(scoped_refptr<media::VideoFrame> frame);

  // A router used to send us IPC messages.
  MessageRouter* router_;

  // Sends IPC messages to the GPU process.
  IPC::Message::Sender* ipc_sender_;

  // Route ID of the GLES2 context in the GPU process.
  int context_route_id_;

  // Message loop that this object runs on.
  MessageLoop* message_loop_;

  // We expect that the client of us will always available during our life span.
  EventHandler* event_handler_;

  // A Context for allocating video frame textures.
  media::VideoDecodeContext* context_;

  // Dimensions of the video.
  int width_;
  int height_;

  // Current state of video decoder.
  GpuVideoDecoderHostState state_;

  // ID of this GpuVideoDecoderHost.
  int32 decoder_host_id_;

  // ID of GpuVideoDecoder in the GPU process.
  int32 decoder_id_;

  // We are not able to push all received buffer to gpu process at once.
  std::deque<scoped_refptr<Buffer> > input_buffer_queue_;

  // Currently we do not use ring buffer in input buffer, therefore before
  // GPU process had finished access it, we should not touch it.
  bool input_buffer_busy_;

  // Transfer buffers for both input and output.
  // TODO(jiesun): remove output buffer when hardware composition is ready.
  scoped_ptr<base::SharedMemory> input_transfer_buffer_;

  // Frame ID for the newly generated video frame.
  int32 current_frame_id_;

  // The list of video frames allocated by VideoDecodeContext.
  std::vector<scoped_refptr<media::VideoFrame> > video_frames_;

  // The mapping between video frame ID and a video frame.
  VideoFrameMap video_frame_map_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecoderHost);
};

#endif  // CHROME_RENDERER_GPU_VIDEO_DECODER_HOST_H_
