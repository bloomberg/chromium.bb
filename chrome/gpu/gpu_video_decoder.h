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
#include "ipc/ipc_channel.h"

class GpuChannel;

class GpuVideoDecoder
    : public IPC::Channel::Listener,
      public base::RefCountedThreadSafe<GpuVideoDecoder> {

 public:
  // IPC::Channel::Listener.
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();
  virtual void OnMessageReceived(const IPC::Message& message);

  virtual bool DoInitialize(const GpuVideoDecoderInitParam& init_param,
                            GpuVideoDecoderInitDoneParam* done_param) = 0;
  virtual bool DoUninitialize() = 0;
  virtual void DoFlush() = 0;
  virtual void DoEmptyThisBuffer(
      const GpuVideoDecoderInputBufferParam& buffer) = 0;
  virtual void DoFillThisBuffer(
      const GpuVideoDecoderOutputBufferParam& frame) = 0;
  virtual void DoFillThisBufferDoneACK() = 0;

  GpuVideoDecoder(const GpuVideoDecoderInfoParam* param,
                  GpuChannel* channel_,
                  base::ProcessHandle handle);
  virtual ~GpuVideoDecoder() {}

 protected:
  // Output message helper.
  void SendInitializeDone(const GpuVideoDecoderInitDoneParam& param);
  void SendUninitializeDone();
  void SendFlushDone();
  void SendEmptyBufferDone();
  void SendEmptyBufferACK();
  void SendFillBufferDone(const GpuVideoDecoderOutputBufferParam& frame);

  int32 route_id() { return decoder_host_route_id_; }

  int32 decoder_host_route_id_;
  GpuChannel* channel_;
  base::ProcessHandle renderer_handle_;

  GpuVideoDecoderInitParam init_param_;
  GpuVideoDecoderInitDoneParam done_param_;

  scoped_ptr<base::SharedMemory> input_transfer_buffer_;
  scoped_ptr<base::SharedMemory> output_transfer_buffer_;

 private:
  // Input message handler.
  void OnInitialize(const GpuVideoDecoderInitParam& param);
  void OnUninitialize();
  void OnFlush();
  void OnEmptyThisBuffer(const GpuVideoDecoderInputBufferParam& buffer);
  void OnFillThisBuffer(const GpuVideoDecoderOutputBufferParam& frame);
  void OnFillThisBufferDoneACK();

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecoder);
};

#endif  // CHROME_GPU_GPU_VIDEO_DECODER_H_

