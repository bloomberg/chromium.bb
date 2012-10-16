// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_WINDOW_ANDROID_HELPER_H_
#define CHROME_BROWSER_UI_ANDROID_WINDOW_ANDROID_HELPER_H_

#include "content/public/browser/web_contents_user_data.h"

namespace ui {
class WindowAndroid;
}

// Per-tab class to provide access to WindowAndroid object.
class WindowAndroidHelper
    : public content::WebContentsUserData<WindowAndroidHelper> {
 public:
  virtual ~WindowAndroidHelper();

  void SetWindowAndroid(ui::WindowAndroid* window_android);
  ui::WindowAndroid* GetWindowAndroid();

 private:
  explicit WindowAndroidHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<WindowAndroidHelper>;

  // The owning window that has a hold of main application activity.
  ui::WindowAndroid* window_android_;

  DISALLOW_COPY_AND_ASSIGN(WindowAndroidHelper);
};

#endif  // CHROME_BROWSER_UI_ANDROID_WINDOW_ANDROID_HELPER_H_
