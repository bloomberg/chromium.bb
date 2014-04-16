// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo/mojo_render_process_observer.h"

#include "base/message_loop/message_loop.h"
#include "content/child/child_process.h"
#include "content/common/mojo/mojo_messages.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/web_ui_mojo.h"
#include "mojo/common/mojo_channel_init.h"

namespace content {

MojoRenderProcessObserver::MojoRenderProcessObserver(
    RenderThread* render_thread)
    : render_thread_(render_thread) {
  render_thread_->AddObserver(this);
}

bool MojoRenderProcessObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MojoRenderProcessObserver, message)
    IPC_MESSAGE_HANDLER(MojoMsg_ChannelCreated, OnChannelCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MojoRenderProcessObserver::OnRenderProcessShutdown() {
  delete this;
}

MojoRenderProcessObserver::~MojoRenderProcessObserver() {
  render_thread_->RemoveObserver(this);
}

void MojoRenderProcessObserver::OnChannelCreated(
    const IPC::PlatformFileForTransit& file) {
#if defined(OS_POSIX)
  base::PlatformFile handle = file.fd;
#elif defined(OS_WIN)
  base::PlatformFile handle = file;
#endif
  DCHECK(!channel_init_.get());
  channel_init_.reset(new mojo::common::MojoChannelInit);
  channel_init_->Init(handle, ChildProcess::current()->io_message_loop_proxy());
  if (!channel_init_->is_handle_valid())
    return;

  ScopedRenderProcessHostMojoHandle render_process_host_handle(
      RenderProcessHostMojoHandle(
          channel_init_->bootstrap_message_pipe().release().value()));
  render_process_host_mojo_.reset(render_process_host_handle.Pass(), this);
}

void MojoRenderProcessObserver::SetWebUIHandle(
    int32 view_routing_id,
    mojo::ScopedMessagePipeHandle web_ui_handle) {
  RenderView* render_view = RenderView::FromRoutingID(view_routing_id);
  if (!render_view)
    return;
  WebUIMojo* web_ui_mojo = WebUIMojo::Get(render_view);
  if (!web_ui_mojo)
    return;
  web_ui_mojo->SetBrowserHandle(web_ui_handle.Pass());
}

}  // namespace content
