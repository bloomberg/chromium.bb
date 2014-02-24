// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_dispatcher_host.h"

#include "content/common/screen_orientation_messages.h"

namespace content {

ScreenOrientationDispatcherHost::ScreenOrientationDispatcherHost() {
}

bool ScreenOrientationDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  // TODO(mlamouri): we will handle lock and unlock requests here.
  return false;
}

void ScreenOrientationDispatcherHost::OnOrientationChange(
    blink::WebScreenOrientation orientation) {
  Send(new ScreenOrientationMsg_OrientationChange(orientation));
}

}  // namespace content
