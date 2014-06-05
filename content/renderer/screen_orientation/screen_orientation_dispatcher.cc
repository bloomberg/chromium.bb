// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/screen_orientation/screen_orientation_dispatcher.h"

#include "content/common/screen_orientation_messages.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationListener.h"

namespace content {

ScreenOrientationDispatcher::ScreenOrientationDispatcher()
    : listener_(NULL) {
  RenderThread::Get()->AddObserver(this);
}

ScreenOrientationDispatcher::~ScreenOrientationDispatcher() {
}

bool ScreenOrientationDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(ScreenOrientationDispatcher, message)
    IPC_MESSAGE_HANDLER(ScreenOrientationMsg_OrientationChange,
                        OnOrientationChange)
    IPC_MESSAGE_HANDLER(ScreenOrientationMsg_LockSuccess,
                        OnLockSuccess)
    IPC_MESSAGE_HANDLER(ScreenOrientationMsg_LockError,
                        OnLockError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ScreenOrientationDispatcher::OnOrientationChange(
    blink::WebScreenOrientationType orientation) {
  if (!listener_)
    return;

  listener_->didChangeScreenOrientation(orientation);
}

void ScreenOrientationDispatcher::OnLockSuccess(
    int request_id,
    unsigned angle,
    blink::WebScreenOrientationType orientation) {
  blink::WebLockOrientationCallback* callback =
      pending_callbacks_.Lookup(request_id);
  if (!callback)
    return;
  callback->onSuccess(angle, orientation);
  pending_callbacks_.Remove(request_id);
}

void ScreenOrientationDispatcher::OnLockError(
    int request_id,
    blink::WebLockOrientationCallback::ErrorType error) {
  blink::WebLockOrientationCallback* callback =
      pending_callbacks_.Lookup(request_id);
  if (!callback)
    return;
  callback->onError(error);
  pending_callbacks_.Remove(request_id);
}

void ScreenOrientationDispatcher::setListener(
    blink::WebScreenOrientationListener* listener) {
  listener_ = listener;
}

void ScreenOrientationDispatcher::CancelPendingLocks() {
  for (CallbackMap::Iterator<blink::WebLockOrientationCallback>
       iterator(&pending_callbacks_); !iterator.IsAtEnd(); iterator.Advance()) {
    iterator.GetCurrentValue()->onError(
        blink::WebLockOrientationCallback::ErrorTypeCanceled);
    pending_callbacks_.Remove(iterator.GetCurrentKey());
  }
}

void ScreenOrientationDispatcher::LockOrientation(
    blink::WebScreenOrientationLockType orientation,
    scoped_ptr<blink::WebLockOrientationCallback> callback) {
  CancelPendingLocks();

  int request_id = pending_callbacks_.Add(callback.release());
  RenderThread::Get()->Send(
      new ScreenOrientationHostMsg_LockRequest(orientation, request_id));
}

void ScreenOrientationDispatcher::UnlockOrientation() {
  CancelPendingLocks();
  RenderThread::Get()->Send(new ScreenOrientationHostMsg_Unlock);
}

}  // namespace content
