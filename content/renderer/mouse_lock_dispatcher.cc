// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mouse_lock_dispatcher.h"

#include "content/common/view_messages.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"

MouseLockDispatcher::MouseLockDispatcher(RenderViewImpl* render_view_impl)
    : content::RenderViewObserver(render_view_impl),
      render_view_impl_(render_view_impl),
      mouse_locked_(false),
      pending_lock_request_(false),
      pending_unlock_request_(false),
      unlocked_by_target_(false),
      target_(NULL) {
}

MouseLockDispatcher::~MouseLockDispatcher() {
}

bool MouseLockDispatcher::LockMouse(LockTarget* target) {
  if (MouseLockedOrPendingAction())
    return false;

  pending_lock_request_ = true;
  target_ = target;

  bool user_gesture =
      render_view_impl_->webview() &&
      render_view_impl_->webview()->mainFrame() &&
      render_view_impl_->webview()->mainFrame()->isProcessingUserGesture();
  Send(new ViewHostMsg_LockMouse(routing_id(),
                                 user_gesture,
                                 unlocked_by_target_));
  unlocked_by_target_ = false;
  return true;
}

void MouseLockDispatcher::UnlockMouse(LockTarget* target) {
  if (target && target == target_ && !pending_unlock_request_) {
    pending_unlock_request_ = true;
    unlocked_by_target_ = true;
    Send(new ViewHostMsg_UnlockMouse(routing_id()));
  }
}

void MouseLockDispatcher::OnLockTargetDestroyed(LockTarget* target) {
  if (target == target_) {
    UnlockMouse(target);
    target_ = NULL;
  }
}

bool MouseLockDispatcher::IsMouseLockedTo(LockTarget* target) {
  return mouse_locked_ && target_ == target;
}

bool MouseLockDispatcher::WillHandleMouseEvent(
    const WebKit::WebMouseEvent& event) {
  if (mouse_locked_ && target_)
    return target_->HandleMouseLockedInputEvent(event);
  return false;
}

bool MouseLockDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MouseLockDispatcher, message)
    IPC_MESSAGE_HANDLER(ViewMsg_LockMouse_ACK, OnLockMouseACK)
    IPC_MESSAGE_HANDLER(ViewMsg_MouseLockLost, OnMouseLockLost)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MouseLockDispatcher::OnLockMouseACK(bool succeeded) {
  DCHECK(!mouse_locked_ && pending_lock_request_);

  mouse_locked_ = succeeded;
  pending_lock_request_ = false;
  if (pending_unlock_request_ && !succeeded) {
    // We have sent an unlock request after the lock request. However, since
    // the lock request has failed, the unlock request will be ignored by the
    // browser side and there won't be any response to it.
    pending_unlock_request_ = false;
  }

  LockTarget* last_target = target_;
  if (!succeeded)
    target_ = NULL;

  // Callbacks made after all state modification to prevent reentrant errors
  // such as OnLockMouseACK() synchronously calling LockMouse().

  if (last_target)
    last_target->OnLockMouseACK(succeeded);

  // Mouse Lock removes the system cursor and provides all mouse motion as
  // .movementX/Y values on events all sent to a fixed target. This requires
  // content to specifically request the mode to be entered.
  // Mouse Capture is implicitly given for the duration of a drag event, and
  // sends all mouse events to the initial target of the drag.
  // If Lock is entered it supercedes any in progress Capture.
  if (succeeded && render_view_impl_->webwidget())
      render_view_impl_->webwidget()->mouseCaptureLost();
}

void MouseLockDispatcher::OnMouseLockLost() {
  DCHECK(mouse_locked_ && !pending_lock_request_);

  mouse_locked_ = false;
  pending_unlock_request_ = false;

  LockTarget* last_target = target_;
  target_ = NULL;

  // Callbacks made after all state modification to prevent reentrant errors
  // such as OnMouseLockLost() synchronously calling LockMouse().

  if (last_target)
    last_target->OnMouseLockLost();
}
