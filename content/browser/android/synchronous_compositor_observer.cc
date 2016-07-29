// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/synchronous_compositor_observer.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "content/browser/android/synchronous_compositor_host.h"
#include "content/browser/bad_message.h"
#include "content/common/android/sync_compositor_messages.h"
#include "content/public/browser/render_process_host.h"
#include "ui/android/window_android.h"

namespace content {

namespace {
base::LazyInstance<std::map<int, SynchronousCompositorObserver*>> g_instances;
}

// static
SynchronousCompositorObserver* SynchronousCompositorObserver::GetOrCreateFor(
    int process_id) {
  auto itr = g_instances.Get().find(process_id);
  if (itr != g_instances.Get().end())
    return itr->second;
  return new SynchronousCompositorObserver(process_id);
}

SynchronousCompositorObserver::SynchronousCompositorObserver(int process_id)
    : render_process_host_(RenderProcessHost::FromID(process_id)),
      window_android_in_vsync_(nullptr) {
  DCHECK(render_process_host_);
  DCHECK(!ContainsKey(g_instances.Get(), render_process_host_->GetID()));
  g_instances.Get()[render_process_host_->GetID()] = this;
  render_process_host_->AddObserver(this);
}

SynchronousCompositorObserver::~SynchronousCompositorObserver() {
  DCHECK(compositor_host_pending_renderer_state_.empty());
  DCHECK(ContainsKey(g_instances.Get(), render_process_host_->GetID()));
  DCHECK_EQ(this, g_instances.Get()[render_process_host_->GetID()]);
  render_process_host_->RemoveObserver(this);
  g_instances.Get().erase(render_process_host_->GetID());
}

void SynchronousCompositorObserver::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  DCHECK_EQ(render_process_host_, host);
  delete this;
}

void SynchronousCompositorObserver::SyncStateAfterVSync(
    ui::WindowAndroid* window_android,
    SynchronousCompositorHost* compositor_host) {
  DCHECK(!window_android_in_vsync_ ||
         window_android_in_vsync_ == window_android)
      << !!window_android_in_vsync_;
  DCHECK(compositor_host);
  DCHECK(
      !ContainsValue(compositor_host_pending_renderer_state_, compositor_host));
  compositor_host_pending_renderer_state_.push_back(compositor_host);
  if (window_android_in_vsync_)
    return;
  window_android_in_vsync_ = window_android;
  window_android_in_vsync_->AddObserver(this);
}

void SynchronousCompositorObserver::OnCompositingDidCommit() {
  NOTREACHED();
}

void SynchronousCompositorObserver::OnRootWindowVisibilityChanged(
    bool visible) {
  NOTREACHED();
}

void SynchronousCompositorObserver::OnAttachCompositor() {
  NOTREACHED();
}

void SynchronousCompositorObserver::OnDetachCompositor() {
  NOTREACHED();
}

void SynchronousCompositorObserver::OnVSync(base::TimeTicks frame_time,
                                            base::TimeDelta vsync_period) {
  // This is called after DidSendBeginFrame for SynchronousCompositorHosts
  // belonging to this WindowAndroid, since this is added as an Observer after
  // the observer iteration has started.
  DCHECK(window_android_in_vsync_);
  window_android_in_vsync_->RemoveObserver(this);
  window_android_in_vsync_ = nullptr;

  std::vector<int> routing_ids;
  routing_ids.reserve(compositor_host_pending_renderer_state_.size());
  for (const auto host : compositor_host_pending_renderer_state_)
    routing_ids.push_back(host->routing_id());

  std::vector<SyncCompositorCommonRendererParams> params;
  params.reserve(compositor_host_pending_renderer_state_.size());

  if (!render_process_host_->Send(
          new SyncCompositorMsg_SynchronizeRendererState(routing_ids,
                                                         &params))) {
    return;
  }

  if (compositor_host_pending_renderer_state_.size() != params.size()) {
    bad_message::ReceivedBadMessage(render_process_host_,
                                    bad_message::SCO_INVALID_ARGUMENT);
    return;
  }

  for (size_t i = 0; i < compositor_host_pending_renderer_state_.size(); ++i) {
    compositor_host_pending_renderer_state_[i]->ProcessCommonParams(params[i]);
  }
  compositor_host_pending_renderer_state_.clear();
}

void SynchronousCompositorObserver::OnActivityStopped() {
  NOTREACHED();
}

void SynchronousCompositorObserver::OnActivityStarted() {
  NOTREACHED();
}

}  // namespace content
