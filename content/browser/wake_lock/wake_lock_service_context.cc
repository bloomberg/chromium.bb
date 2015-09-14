// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/wake_lock/wake_lock_service_context.h"

#include "base/bind.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/power_save_blocker_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/power_save_blocker.h"
#include "content/public/common/service_registry.h"

namespace content {

WakeLockServiceContext::WakeLockServiceContext(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      weak_factory_(this) {
}

WakeLockServiceContext::~WakeLockServiceContext() {
}

void WakeLockServiceContext::CreateService(
    int frame_id,
    mojo::InterfaceRequest<WakeLockService> request) {
  new WakeLockServiceImpl(weak_factory_.GetWeakPtr(),
                          frame_id,
                          request.Pass());
}

void WakeLockServiceContext::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  RenderFrameHostImpl* rfh_impl =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  CancelWakeLock(rfh_impl->frame_tree_node()->frame_tree_node_id());
}

void WakeLockServiceContext::RequestWakeLock(int frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  frames_requesting_lock_.insert(frame_id);
  UpdateWakeLock();
}

void WakeLockServiceContext::CancelWakeLock(int frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  frames_requesting_lock_.erase(frame_id);
  UpdateWakeLock();
}

bool WakeLockServiceContext::HasWakeLockForTests() const {
  return wake_lock_;
}

void WakeLockServiceContext::CreateWakeLock() {
  DCHECK(!wake_lock_);
  wake_lock_ = PowerSaveBlocker::Create(
      PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep,
      PowerSaveBlocker::kReasonOther,
      "Wake Lock API");

  // On Android, additionaly associate the blocker with this WebContents.
#if defined(OS_ANDROID)
  DCHECK(web_contents());

  static_cast<PowerSaveBlockerImpl*>(wake_lock_.get())->InitDisplaySleepBlocker(
      web_contents());
#endif
}

void WakeLockServiceContext::RemoveWakeLock() {
  DCHECK(wake_lock_);
  wake_lock_.reset();
}

void WakeLockServiceContext::UpdateWakeLock() {
  if (!frames_requesting_lock_.empty()) {
    if (!wake_lock_)
      CreateWakeLock();
  } else {
    if (wake_lock_)
      RemoveWakeLock();
  }
}

}  // namespace content
