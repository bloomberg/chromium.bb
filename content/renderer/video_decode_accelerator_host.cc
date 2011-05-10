// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/video_decode_accelerator_host.h"

#include "base/logging.h"
#include "base/task.h"

using media::VideoDecodeAccelerator;
using media::VideoDecodeAcceleratorCallback;

VideoDecodeAcceleratorHost::VideoDecodeAcceleratorHost(
    MessageRouter* router,
    IPC::Message::Sender* ipc_sender,
    int32 decoder_host_id)
    : message_loop_(NULL),
      router_(router),
      ipc_sender_(ipc_sender),
      context_route_id_(0),
      decoder_host_id_(decoder_host_id),
      decoder_id_(0) {
}

VideoDecodeAcceleratorHost::~VideoDecodeAcceleratorHost() {}

void VideoDecodeAcceleratorHost::OnChannelConnected(int32 peer_pid) {}

void VideoDecodeAcceleratorHost::OnChannelError() {
  ipc_sender_ = NULL;
}

bool VideoDecodeAcceleratorHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  DCHECK(handled);
  return handled;
}

void VideoDecodeAcceleratorHost::GetConfigs(
    const std::vector<uint32>& requested_configs,
    std::vector<uint32>* matched_configs) {
  // TODO(vmr): implement.
  NOTIMPLEMENTED();
}

bool VideoDecodeAcceleratorHost::Initialize(
    const std::vector<uint32>& config) {
  // TODO(vmr): implement.
  NOTIMPLEMENTED();
  return false;
}

bool VideoDecodeAcceleratorHost::Decode(
    const media::BitstreamBuffer& bitstream_buffer,
    VideoDecodeAcceleratorCallback* callback) {
  // TODO(vmr): implement.
  NOTIMPLEMENTED();
  return false;
}

void VideoDecodeAcceleratorHost::AssignGLESBuffers(
    const std::vector<media::GLESBuffer>& buffers) {
  // TODO(vmr): implement.
  NOTIMPLEMENTED();
}

void VideoDecodeAcceleratorHost::AssignSysmemBuffers(
    const std::vector<media::SysmemBuffer>& buffers) {
  // TODO(vmr): implement.
  NOTIMPLEMENTED();
}

void VideoDecodeAcceleratorHost::ReusePictureBuffer(uint32 picture_buffer_id) {
  // TODO(vmr): implement.
  NOTIMPLEMENTED();
}

bool VideoDecodeAcceleratorHost::Flush(
    VideoDecodeAcceleratorCallback* callback) {
  // TODO(vmr): implement.
  NOTIMPLEMENTED();
  return false;
}

bool VideoDecodeAcceleratorHost::Abort(
    VideoDecodeAcceleratorCallback* callback) {
  // TODO(vmr): implement.
  NOTIMPLEMENTED();
  return false;
}

DISABLE_RUNNABLE_METHOD_REFCOUNT(VideoDecodeAcceleratorHost);
