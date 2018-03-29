// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/synchronous_compositor_browser_filter.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "content/browser/android/synchronous_compositor_sync_call_bridge.h"
#include "content/browser/bad_message.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

SynchronousCompositorBrowserFilter::SynchronousCompositorBrowserFilter(
    int process_id)
    : BrowserMessageFilter(SyncCompositorMsgStart),
      render_process_host_(RenderProcessHost::FromID(process_id)) {
  DCHECK(render_process_host_);
}

SynchronousCompositorBrowserFilter::~SynchronousCompositorBrowserFilter() {
}

bool SynchronousCompositorBrowserFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SynchronousCompositorBrowserFilter, message)
    IPC_MESSAGE_HANDLER_GENERIC(SyncCompositorHostMsg_ReturnFrame,
                                ReceiveFrame(message))
    IPC_MESSAGE_HANDLER_GENERIC(SyncCompositorHostMsg_BeginFrameResponse,
                                BeginFrameResponse(message))
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool SynchronousCompositorBrowserFilter::ReceiveFrame(
    const IPC::Message& message) {
  SyncCompositorHostMsg_ReturnFrame::Param param;
  if (!SyncCompositorHostMsg_ReturnFrame::Read(&message, &param))
    return false;

  int routing_id = message.routing_id();
  auto itr = bridges_.find(routing_id);
  if (itr == bridges_.end()) {
    bad_message::ReceivedBadMessage(this, bad_message::SCO_INVALID_ARGUMENT);
    return true;
  }

  int frame_sink_id = std::get<0>(param);
  uint32_t metadata_version = std::get<1>(param);
  base::Optional<viz::CompositorFrame>& compositor_frame = std::get<2>(param);

  if (!itr->second->ReceiveFrameOnIOThread(frame_sink_id, metadata_version,
                                           std::move(compositor_frame))) {
    bad_message::ReceivedBadMessage(this, bad_message::SCO_INVALID_ARGUMENT);
  }
  return true;
}

bool SynchronousCompositorBrowserFilter::BeginFrameResponse(
    const IPC::Message& message) {
  SyncCompositorHostMsg_BeginFrameResponse::Param param;
  if (!SyncCompositorHostMsg_BeginFrameResponse::Read(&message, &param))
    return false;

  int routing_id = message.routing_id();
  auto itr = bridges_.find(routing_id);
  if (itr == bridges_.end()) {
    bad_message::ReceivedBadMessage(this, bad_message::SCO_INVALID_ARGUMENT);
    return true;
  }
  const SyncCompositorCommonRendererParams& render_params = std::get<0>(param);
  if (!itr->second->BeginFrameResponseOnIOThread(render_params))
    bad_message::ReceivedBadMessage(this, bad_message::SCO_INVALID_ARGUMENT);
  return true;
}

void SynchronousCompositorBrowserFilter::OnFilterAdded(IPC::Channel* channel) {
  filter_ready_ = true;
  for (auto& bridge : bridges_)
    bridge.second->RemoteReady();
}

void SynchronousCompositorBrowserFilter::OnFilterRemoved() {
  SignalFilterClosed();
}

void SynchronousCompositorBrowserFilter::OnChannelClosing() {
  SignalFilterClosed();
}

void SynchronousCompositorBrowserFilter::OnChannelError() {
  SignalFilterClosed();
}

void SynchronousCompositorBrowserFilter::SignalFilterClosed() {
  for (auto& bridge : bridges_)
    bridge.second->RemoteClosedOnIOThread();
}

void SynchronousCompositorBrowserFilter::RegisterSyncCallBridge(
    int routing_id,
    scoped_refptr<SynchronousCompositorSyncCallBridge> host_bridge) {
  DCHECK(bridges_.find(routing_id) == bridges_.end());
  bridges_[routing_id] = host_bridge;
  if (filter_ready_)
    host_bridge->RemoteReady();
}

void SynchronousCompositorBrowserFilter::UnregisterSyncCallBridge(
    int routing_id) {
  DCHECK(bridges_.find(routing_id) != bridges_.end());
  bridges_.erase(routing_id);
}

}  // namespace content
