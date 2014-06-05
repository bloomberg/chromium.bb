// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/screen_orientation/screen_orientation_dispatcher.h"

#include "content/common/screen_orientation_messages.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationListener.h"

namespace content {

ScreenOrientationDispatcher::ScreenOrientationDispatcher(
    RenderThread* thread)
  : listener_(NULL) {
    thread->AddObserver(this);
}

bool ScreenOrientationDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(ScreenOrientationDispatcher, message)
    IPC_MESSAGE_HANDLER(ScreenOrientationMsg_OrientationChange,
                        OnOrientationChange)
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

void ScreenOrientationDispatcher::setListener(
    blink::WebScreenOrientationListener* listener) {
  listener_ = listener;
}

}  // namespace content
