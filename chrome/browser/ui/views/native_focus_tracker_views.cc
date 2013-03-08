// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_focus_tracker_views.h"

NativeFocusTrackerViews::NativeFocusTrackerViews(NativeFocusTrackerHost* host)
    : host_(host) {
  views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
}

NativeFocusTrackerViews::~NativeFocusTrackerViews() {
  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
}

void NativeFocusTrackerViews::OnNativeFocusChange(
    gfx::NativeView focused_before,
    gfx::NativeView focused_now) {
  host_->SetBrowser(GetBrowserForNativeView(focused_now));
}

// static
NativeFocusTracker* NativeFocusTracker::Create(NativeFocusTrackerHost* host) {
  return new NativeFocusTrackerViews(host);
}
