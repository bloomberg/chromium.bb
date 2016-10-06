// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/stream_texture_factory.h"

#include "base/macros.h"
#include "cc/output/context_provider.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "ui/gfx/geometry/size.h"

namespace content {

StreamTextureProxy::StreamTextureProxy(StreamTextureHost* host) : host_(host) {}

StreamTextureProxy::~StreamTextureProxy() {}

void StreamTextureProxy::Release() {
  {
    // Cannot call |received_frame_cb_| after returning from here.
    base::AutoLock lock(lock_);
    received_frame_cb_.Reset();
  }
  // Release is analogous to the destructor, so there should be no more external
  // calls to this object in Release. Therefore there is no need to acquire the
  // lock to access |task_runner_|.
  if (!task_runner_.get() || task_runner_->BelongsToCurrentThread() ||
      !task_runner_->DeleteSoon(FROM_HERE, this)) {
    delete this;
  }
}

void StreamTextureProxy::BindToTaskRunner(
    int32_t stream_id,
    const base::Closure& received_frame_cb,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner.get());

  {
    base::AutoLock lock(lock_);
    DCHECK(!task_runner_.get() || (task_runner.get() == task_runner_.get()));
    task_runner_ = task_runner;
    received_frame_cb_ = received_frame_cb;
  }

  if (task_runner->BelongsToCurrentThread()) {
    BindOnThread(stream_id);
    return;
  }
  // Unretained is safe here only because the object is deleted on |loop_|
  // thread.
  task_runner->PostTask(FROM_HERE,
                        base::Bind(&StreamTextureProxy::BindOnThread,
                                   base::Unretained(this), stream_id));
}

void StreamTextureProxy::BindOnThread(int32_t stream_id) {
  host_->BindToCurrentThread(stream_id, this);
}

void StreamTextureProxy::OnFrameAvailable() {
  base::AutoLock lock(lock_);
  if (!received_frame_cb_.is_null())
    received_frame_cb_.Run();
}

// static
scoped_refptr<StreamTextureFactory> StreamTextureFactory::Create(
    scoped_refptr<ContextProviderCommandBuffer> context_provider) {
  return new StreamTextureFactory(std::move(context_provider));
}

StreamTextureFactory::StreamTextureFactory(
    scoped_refptr<ContextProviderCommandBuffer> context_provider)
    : context_provider_(std::move(context_provider)),
      channel_(context_provider_->GetCommandBufferProxy()->channel()) {
  DCHECK(channel_);
}

StreamTextureFactory::~StreamTextureFactory() {}

StreamTextureProxy* StreamTextureFactory::CreateProxy() {
  StreamTextureHost* host = new StreamTextureHost(channel_);
  return new StreamTextureProxy(host);
}

void StreamTextureFactory::EstablishPeer(int32_t stream_id,
                                             int player_id,
                                             int frame_id) {
  channel_->Send(
      new GpuStreamTextureMsg_EstablishPeer(stream_id, frame_id, player_id));
}

void StreamTextureFactory::ForwardStreamTextureForSurfaceRequest(
    int32_t stream_id,
    const base::UnguessableToken& request_token) {
  channel_->Send(new GpuStreamTextureMsg_ForwardForSurfaceRequest(
      stream_id, request_token));
}

unsigned StreamTextureFactory::CreateStreamTexture(
    unsigned texture_target,
    unsigned* texture_id,
    gpu::Mailbox* texture_mailbox) {
  GLuint stream_id = 0;
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->GenTextures(1, texture_id);
  gl->ShallowFlushCHROMIUM();
  stream_id = context_provider_->GetCommandBufferProxy()->CreateStreamTexture(
      *texture_id);
  gl->GenMailboxCHROMIUM(texture_mailbox->name);
  gl->ProduceTextureDirectCHROMIUM(
      *texture_id, texture_target, texture_mailbox->name);
  return stream_id;
}

void StreamTextureFactory::SetStreamTextureSize(int32_t stream_id,
                                                    const gfx::Size& size) {
  channel_->Send(new GpuStreamTextureMsg_SetSize(stream_id, size));
}

gpu::gles2::GLES2Interface* StreamTextureFactory::ContextGL() {
  return context_provider_->ContextGL();
}

}  // namespace content
