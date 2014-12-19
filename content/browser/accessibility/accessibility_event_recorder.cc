// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/port.h"
#include "content/browser/accessibility/accessibility_event_recorder.h"

namespace content {

AccessibilityEventRecorder::AccessibilityEventRecorder(
    BrowserAccessibilityManager* manager)
    : manager_(manager) {
}

AccessibilityEventRecorder::~AccessibilityEventRecorder() {
}

#if !defined(OS_WIN) && !defined(OS_MACOSX)
// static
AccessibilityEventRecorder* AccessibilityEventRecorder::Create(
    BrowserAccessibilityManager* manager) {
  return new AccessibilityEventRecorder(manager);
}
#endif

}  // namespace content
