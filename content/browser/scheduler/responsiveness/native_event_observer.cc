// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/native_event_observer.h"

namespace content {
namespace responsiveness {

NativeEventObserver::NativeEventObserver(
    WillRunEventCallback will_run_event_callback,
    DidRunEventCallback did_run_event_callback)
    : will_run_event_callback_(will_run_event_callback),
      did_run_event_callback_(did_run_event_callback) {
  RegisterObserver();
}

NativeEventObserver::~NativeEventObserver() {
  DeregisterObserver();
}

#if !defined(OS_MACOSX)
// TODO(erikchen): Implement this for non-macOS platforms.
// https://crbug.com/859155.
void NativeEventObserver::RegisterObserver() {}
void NativeEventObserver::DeregisterObserver() {}
#endif

}  // namespace responsiveness
}  // namespace content
