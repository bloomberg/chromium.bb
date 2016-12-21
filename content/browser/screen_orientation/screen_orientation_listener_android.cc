// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_listener_android.h"

#include "base/message_loop/message_loop.h"
#include "content/browser/screen_orientation/screen_orientation_delegate_android.h"
#include "content/common/screen_orientation_messages.h"

namespace content {

ScreenOrientationListenerAndroid::ScreenOrientationListenerAndroid()
    : BrowserMessageFilter(ScreenOrientationMsgStart),
      BrowserAssociatedInterface<device::mojom::ScreenOrientationListener>(
          this,
          this),
      listeners_count_(0) {}

ScreenOrientationListenerAndroid::~ScreenOrientationListenerAndroid() {
  DCHECK(base::MessageLoopForIO::IsCurrent());
  if (listeners_count_ > 0)
    ScreenOrientationDelegateAndroid::StopAccurateListening();
}

bool ScreenOrientationListenerAndroid::OnMessageReceived(
    const IPC::Message& message) {
  return false;
}

void ScreenOrientationListenerAndroid::Start() {
  DCHECK(base::MessageLoopForIO::IsCurrent());
  ++listeners_count_;
  if (listeners_count_ == 1)
    ScreenOrientationDelegateAndroid::StartAccurateListening();
}

void ScreenOrientationListenerAndroid::Stop() {
  DCHECK(base::MessageLoopForIO::IsCurrent());
  DCHECK(listeners_count_ > 0);
  --listeners_count_;
  if (listeners_count_ == 0)
    ScreenOrientationDelegateAndroid::StopAccurateListening();
}

}  // namespace content
