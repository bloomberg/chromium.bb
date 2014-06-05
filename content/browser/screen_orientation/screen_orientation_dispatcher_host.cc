// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_dispatcher_host.h"

#include "content/browser/screen_orientation/screen_orientation_provider.h"
#include "content/common/screen_orientation_messages.h"

namespace content {

ScreenOrientationDispatcherHost::ScreenOrientationDispatcherHost()
  : BrowserMessageFilter(ScreenOrientationMsgStart) {
  if (!provider_.get())
    provider_.reset(CreateProvider());
}

ScreenOrientationDispatcherHost::~ScreenOrientationDispatcherHost() {
}

bool ScreenOrientationDispatcherHost::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(ScreenOrientationDispatcherHost, message)
    IPC_MESSAGE_HANDLER(ScreenOrientationHostMsg_Lock, OnLockRequest)
    IPC_MESSAGE_HANDLER(ScreenOrientationHostMsg_Unlock, OnUnlockRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ScreenOrientationDispatcherHost::OnOrientationChange(
    blink::WebScreenOrientationType orientation) {
  Send(new ScreenOrientationMsg_OrientationChange(orientation));
}

void ScreenOrientationDispatcherHost::SetProviderForTests(
    ScreenOrientationProvider* provider) {
  provider_.reset(provider);
}

void ScreenOrientationDispatcherHost::OnLockRequest(
    blink::WebScreenOrientationLockType orientation) {
  if (!provider_.get())
    return;

  provider_->LockOrientation(orientation);
}

void ScreenOrientationDispatcherHost::OnUnlockRequest() {
  if (!provider_.get())
    return;

  provider_->UnlockOrientation();
}

#if !defined(OS_ANDROID)
// static
ScreenOrientationProvider* ScreenOrientationDispatcherHost::CreateProvider() {
  return NULL;
}
#endif

}  // namespace content
