// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_NAVIGATION_POPUP_H_
#define CHROME_BROWSER_UI_ANDROID_NAVIGATION_POPUP_H_

#include <jni.h>

#include "base/android/jni_helper.h"
#include "base/basictypes.h"
#include "chrome/common/cancelable_task_tracker.h"

class GURL;

namespace history {
struct FaviconImageResult;
}

// The native bridge for the history navigation popup that provides additional
// functionality to the UI widget.
class NavigationPopup {
 public:
  NavigationPopup(JNIEnv* env, jobject obj);

  void Destroy(JNIEnv* env, jobject obj);
  void FetchFaviconForUrl(JNIEnv* env, jobject obj, jstring jurl);

  void OnFaviconDataAvailable(GURL navigation_entry_url,
                              const history::FaviconImageResult& image_result);

  static bool RegisterNavigationPopup(JNIEnv* env);

 protected:
  virtual ~NavigationPopup();

 private:
  JavaObjectWeakGlobalRef weak_jobject_;

  CancelableTaskTracker cancelable_task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(NavigationPopup);
};

#endif  // CHROME_BROWSER_UI_ANDROID_NAVIGATION_POPUP_H_
