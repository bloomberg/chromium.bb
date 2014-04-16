// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_process_host_mojo_impl.h"

#include "base/platform_file.h"
#include "content/common/mojo/mojo_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/common/mojo_channel_init.h"
#include "mojo/embedder/platform_channel_pair.h"

namespace content {

namespace {

base::PlatformFile PlatformFileFromScopedPlatformHandle(
    mojo::embedder::ScopedPlatformHandle handle) {
#if defined(OS_POSIX)
  return handle.release().fd;
#elif defined(OS_WIN)
  return handle.release().handle;
#endif
}

}  // namespace

struct RenderProcessHostMojoImpl::PendingHandle {
  PendingHandle() : view_routing_id(0) {}

  int32 view_routing_id;
  mojo::ScopedMessagePipeHandle handle;
};

RenderProcessHostMojoImpl::RenderProcessHostMojoImpl(RenderProcessHost* host)
    : host_(host) {
}

RenderProcessHostMojoImpl::~RenderProcessHostMojoImpl() {
}

void RenderProcessHostMojoImpl::SetWebUIHandle(
    int32 view_routing_id,
    mojo::ScopedMessagePipeHandle handle) {
  base::ProcessHandle process = host_->GetHandle();
  if (process != base::kNullProcessHandle) {
    CreateMojoChannel(process);  // Does nothing if already connected.
    if (!render_process_mojo_.is_null()) {
      render_process_mojo_->SetWebUIHandle(view_routing_id, handle.Pass());
      return;
    }
  }

  // Remember the request, we'll attempt to reconnect once the child process is
  // launched.
  pending_handle_.reset(new PendingHandle);
  pending_handle_->view_routing_id = view_routing_id;
  pending_handle_->handle = handle.Pass();
}

void RenderProcessHostMojoImpl::OnProcessLaunched() {
  if (pending_handle_) {
    scoped_ptr<PendingHandle> handle(pending_handle_.Pass());
    SetWebUIHandle(handle->view_routing_id, handle->handle.Pass());
  }
}


void RenderProcessHostMojoImpl::CreateMojoChannel(
    base::ProcessHandle process_handle) {
  if (mojo_channel_init_.get())
    return;

  mojo::embedder::PlatformChannelPair channel_pair;
  mojo_channel_init_.reset(new mojo::common::MojoChannelInit);
  mojo_channel_init_->Init(
      PlatformFileFromScopedPlatformHandle(channel_pair.PassServerHandle()),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  if (!mojo_channel_init_->is_handle_valid())
    return;
  base::PlatformFile client_file =
      PlatformFileFromScopedPlatformHandle(channel_pair.PassClientHandle());
  host_->Send(new MojoMsg_ChannelCreated(
                  IPC::GetFileHandleForProcess(client_file, process_handle,
                                               true)));
  ScopedRenderProcessMojoHandle render_process_handle(
      RenderProcessMojoHandle(
          mojo_channel_init_->bootstrap_message_pipe().release().value()));
  render_process_mojo_.reset(render_process_handle.Pass(), this);
}

}  // namespace content
