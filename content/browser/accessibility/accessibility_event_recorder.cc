// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_event_recorder.h"

#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

AccessibilityEventRecorder::AccessibilityEventRecorder(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid)
    : manager_(manager) {}

AccessibilityEventRecorder::~AccessibilityEventRecorder() = default;

#if !defined(OS_WIN) && !defined(OS_MACOSX)
// static
AccessibilityEventRecorder& AccessibilityEventRecorder::GetInstance(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid) {
  static base::NoDestructor<AccessibilityEventRecorder> instance(manager, pid);
  return *instance;
}
#endif

void AccessibilityEventRecorder::OnEvent(const std::string& event) {
  event_logs_.push_back(event);
  if (callback_)
    callback_.Run(event);
}

}  // namespace content
