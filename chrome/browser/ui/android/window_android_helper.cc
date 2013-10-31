// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/window_android_helper.h"

#include "content/public/browser/web_contents.h"
#include "ui/base/android/window_android.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(WindowAndroidHelper);

WindowAndroidHelper::WindowAndroidHelper(content::WebContents* web_contents) {
}

WindowAndroidHelper::~WindowAndroidHelper() {
}

void WindowAndroidHelper::SetWindowAndroid(ui::WindowAndroid* window_android) {
  window_android_ = window_android;
}

ui::WindowAndroid* WindowAndroidHelper::GetWindowAndroid() {
  return window_android_;
}
