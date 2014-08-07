// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_dispatcher_host.h"

#include "content/browser/screen_orientation/screen_orientation_provider.h"
#include "content/common/screen_orientation_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"

namespace content {

ScreenOrientationDispatcherHost::LockInformation::LockInformation(
    int request_id, int process_id, int routing_id)
    : request_id(request_id),
      process_id(process_id),
      routing_id(routing_id) {
}

ScreenOrientationDispatcherHost::ScreenOrientationDispatcherHost(
    WebContents* web_contents)
  : WebContentsObserver(web_contents),
    current_lock_(NULL) {
  provider_.reset(ScreenOrientationProvider::Create(this, web_contents));
}

ScreenOrientationDispatcherHost::~ScreenOrientationDispatcherHost() {
  ResetCurrentLock();
}

void ScreenOrientationDispatcherHost::ResetCurrentLock() {
  if (current_lock_) {
    delete current_lock_;
    current_lock_ = 0;
  }
}

bool ScreenOrientationDispatcherHost::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(ScreenOrientationDispatcherHost, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(ScreenOrientationHostMsg_LockRequest, OnLockRequest)
    IPC_MESSAGE_HANDLER(ScreenOrientationHostMsg_Unlock, OnUnlockRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

RenderFrameHost*
ScreenOrientationDispatcherHost::GetRenderFrameHostForRequestID(
    int request_id) {
  if (!current_lock_ || current_lock_->request_id != request_id)
    return NULL;

  return RenderFrameHost::FromID(current_lock_->process_id,
                                 current_lock_->routing_id);
}

void ScreenOrientationDispatcherHost::NotifyLockSuccess(int request_id) {
  RenderFrameHost* render_frame_host =
      GetRenderFrameHostForRequestID(request_id);
  if (!render_frame_host)
    return;

  render_frame_host->Send(new ScreenOrientationMsg_LockSuccess(
      render_frame_host->GetRoutingID(), request_id));
  ResetCurrentLock();
}

void ScreenOrientationDispatcherHost::NotifyLockError(
    int request_id, blink::WebLockOrientationError error) {
  RenderFrameHost* render_frame_host =
      GetRenderFrameHostForRequestID(request_id);
  if (!render_frame_host)
    return;

  NotifyLockError(request_id, render_frame_host, error);
}

void ScreenOrientationDispatcherHost::NotifyLockError(
    int request_id,
    RenderFrameHost* render_frame_host,
    blink::WebLockOrientationError error) {
  render_frame_host->Send(new ScreenOrientationMsg_LockError(
      render_frame_host->GetRoutingID(), request_id, error));
  ResetCurrentLock();
}

void ScreenOrientationDispatcherHost::OnOrientationChange() {
  if (provider_)
    provider_->OnOrientationChange();
}

void ScreenOrientationDispatcherHost::OnLockRequest(
    RenderFrameHost* render_frame_host,
    blink::WebScreenOrientationLockType orientation,
    int request_id) {
  if (current_lock_) {
    NotifyLockError(current_lock_->request_id, render_frame_host,
                    blink::WebLockOrientationErrorCanceled);
  }

  if (!provider_) {
    NotifyLockError(request_id, render_frame_host,
                    blink::WebLockOrientationErrorNotAvailable);
    return;
  }

  current_lock_ = new LockInformation(request_id,
                                      render_frame_host->GetProcess()->GetID(),
                                      render_frame_host->GetRoutingID());

  provider_->LockOrientation(request_id, orientation);
}

void ScreenOrientationDispatcherHost::OnUnlockRequest(
    RenderFrameHost* render_frame_host) {
  if (current_lock_) {
    NotifyLockError(current_lock_->request_id, render_frame_host,
                    blink::WebLockOrientationErrorCanceled);
  }

  if (!provider_)
    return;

  provider_->UnlockOrientation();
}

}  // namespace content
