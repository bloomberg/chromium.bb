// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/screen_orientation/screen_orientation_observer.h"

#include "content/renderer/render_thread_impl.h"

namespace content {

ScreenOrientationObserver::ScreenOrientationObserver() {
}

ScreenOrientationObserver::~ScreenOrientationObserver() {
  StopIfObserving();
}

void ScreenOrientationObserver::Start(
    blink::WebPlatformEventListener* listener) {
  // This should never be called with a proper listener.
  CHECK(listener == 0);
  PlatformEventObserver<blink::WebPlatformEventListener>::Start(0);
}

void ScreenOrientationObserver::SendStartMessage() {
  GetScreenOrientationListener()->Start();
}

void ScreenOrientationObserver::SendStopMessage() {
  GetScreenOrientationListener()->Stop();
}

device::mojom::ScreenOrientationListener*
ScreenOrientationObserver::GetScreenOrientationListener() {
  if (!listener_) {
    RenderThreadImpl::current()->GetChannel()->GetRemoteAssociatedInterface(
        &listener_);
  }
  return listener_.get();
}

} // namespace content
