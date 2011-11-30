// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <algorithm>

#include "content/renderer/gpu/transport_texture_host.h"

// On Mac gl2.h clashes with gpu_messages.h and this problem hasn't been
// solved yet so exclude building on Mac.
#if !defined(OS_MACOSX)

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/renderer/gpu/renderer_gl_context.h"
#include "content/renderer/gpu/transport_texture_service.h"
#include "third_party/khronos/GLES2/gl2.h"

TransportTextureHost::TransportTextureHost(MessageLoop* io_message_loop,
                                           MessageLoop* render_message_loop,
                                           TransportTextureService* service,
                                           IPC::Message::Sender* sender,
                                           RendererGLContext* context,
                                           int32 context_route_id,
                                           int32 host_id)
    : io_message_loop_(io_message_loop),
      render_message_loop_(render_message_loop),
      service_(service),
      sender_(sender),
      context_(context),
      context_route_id_(context_route_id),
      host_id_(host_id),
      peer_id_(0) {
}

TransportTextureHost::~TransportTextureHost() {
}

void TransportTextureHost::Init(Task* done_task) {
  if (MessageLoop::current() != io_message_loop_) {
    io_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&TransportTextureHost::Init, this, done_task));
    return;
  }

  init_task_.reset(done_task);

  // Send the message.
  bool ret = sender_->Send(
      new GpuChannelMsg_CreateTransportTexture(context_route_id_, host_id_));
  if (!ret) {
    LOG(ERROR) << "GpuChannelMsg_CreateTransportTexture failed";
    init_task_->Run();
    init_task_.reset();
  }
}

void TransportTextureHost::Destroy() {
  ReleaseTexturesInternal();
  SendDestroyInternal();

  service_->RemoveRoute(host_id_);
}

void TransportTextureHost::GetTextures(TextureUpdateCallback* callback,
                                       std::vector<int>* textures) {
  textures->resize(textures_.size());
  std::copy(textures_.begin(), textures_.end(), textures->begin());
  update_callback_.reset(callback);
}

int TransportTextureHost::GetPeerId() {
  return peer_id_;
}

void TransportTextureHost::OnChannelConnected(int32 peer_pid) {
}

void TransportTextureHost::OnChannelError() {
}

bool TransportTextureHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TransportTextureHost, msg)
    IPC_MESSAGE_HANDLER(GpuTransportTextureHostMsg_TransportTextureCreated,
                        OnTransportTextureCreated)
    IPC_MESSAGE_HANDLER(GpuTransportTextureHostMsg_CreateTextures,
                        OnCreateTextures)
    IPC_MESSAGE_HANDLER(GpuTransportTextureHostMsg_ReleaseTextures,
                        OnReleaseTextures)
    IPC_MESSAGE_HANDLER(GpuTransportTextureHostMsg_TextureUpdated,
                        OnTextureUpdated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void TransportTextureHost::ReleaseTexturesInternal() {
  if (MessageLoop::current() != render_message_loop_) {
    render_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&TransportTextureHost::ReleaseTexturesInternal, this));
    return;
  }

  scoped_array<GLuint> textures(new GLuint[textures_.size()]);
  for (size_t i = 0; i < textures_.size(); ++i)
    textures[i] = textures_[i];
  glDeleteTextures(textures_.size(), textures.get());
}

void TransportTextureHost::SendTexturesInternal(
    const std::vector<int>& textures) {
  if (MessageLoop::current() != io_message_loop_) {
    io_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&TransportTextureHost::SendTexturesInternal, this,
                   textures));
    return;
  }

  bool ret = sender_->Send(
      new GpuTransportTextureMsg_TexturesCreated(peer_id_, textures));
  if (!ret) {
    LOG(ERROR) << "GpuTransportTextureMsg_TexturesCreated failed";
  }
}

void TransportTextureHost::SendDestroyInternal() {
  if (MessageLoop::current() != io_message_loop_) {
    io_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&TransportTextureHost::SendDestroyInternal, this));
    return;
  }

  bool ret = sender_->Send(new GpuTransportTextureMsg_Destroy(peer_id_));
  if (!ret) {
    LOG(ERROR) << "GpuTransportTextureMsg_Destroy failed";
  }
}

void TransportTextureHost::OnTransportTextureCreated(int32 peer_id) {
  DCHECK_EQ(io_message_loop_, MessageLoop::current());

  peer_id_ = peer_id;
  init_task_->Run();
  init_task_.reset();
}

void TransportTextureHost::OnCreateTextures(int32 n, uint32 width,
                                            uint32 height, int32 format) {
  if (MessageLoop::current() != render_message_loop_) {
    render_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&TransportTextureHost::OnCreateTextures, this, n, width,
                   height, format));
    return;
  }

  // In this method we need to make the ggl context current and then generate
  // textures for each video frame. We also need to allocate memory for each
  // texture generated.
  bool ret = RendererGLContext::MakeCurrent(context_);
  CHECK(ret) << "Failed to switch context";

  // TODO(hclam): Should do this through TextureManager instead of generating
  // textures directly.
  scoped_array<GLuint> textures(new GLuint[n]);
  glGenTextures(n, textures.get());
  for (int i = 0; i < n; ++i) {
    textures_.push_back(textures[i]);
  }
  glFinish();

  // Send textures to the GPU process.
  SendTexturesInternal(textures_);
}

void TransportTextureHost::OnReleaseTextures() {
  // This method will switch thread properly so just call it directly.
  ReleaseTexturesInternal();
}

void TransportTextureHost::OnTextureUpdated(int texture_id) {
  if (update_callback_.get())
    update_callback_->Run(texture_id);
}

#endif
