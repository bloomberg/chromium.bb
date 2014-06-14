// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
#define CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_

#include "base/android/jni_weak_ref.h"
#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/android/tab_android.h"
#include "content/public/browser/web_contents_observer.h"

namespace favicon_base {
struct FaviconRawBitmapResult;
}  // namespace favicon_base

namespace content {
class WebContents;
}  // namespace content

namespace IPC {
class Message;
}

class GURL;

// Adds a shortcut to the current URL to the Android home screen.
// This proceeds over three phases:
// 1) The renderer is asked to parse out webapp related meta tags with an async
//    IPC message.
// 2) The highest-resolution favicon available is retrieved for use as the
//    icon on the home screen.
// 3) A JNI call is made to fire an Intent at the Android launcher, which adds
//    the shortcut.
class ShortcutBuilder : public content::WebContentsObserver {
 public:
  enum ShortcutType {
    APP_SHORTCUT,
    APP_SHORTCUT_APPLE,
    BOOKMARK
  };

  explicit ShortcutBuilder(content::WebContents* web_contents,
                           const base::string16& title,
                           int launcher_large_icon_size);
  virtual ~ShortcutBuilder() {}

  void OnDidRetrieveWebappInformation(bool success,
                                      bool is_mobile_webapp_capable,
                                      bool is_apple_mobile_webapp_capable,
                                      const GURL& expected_url);

  void FinishAddingShortcut(
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // WebContentsObserver
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;

 private:
  void Destroy();

  GURL url_;
  base::string16 title_;
  int launcher_large_icon_size_;
  ShortcutType shortcut_type_;
  base::CancelableTaskTracker cancelable_task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutBuilder);
};

class ShortcutHelper {
 public:
  // Adds a shortcut to the current URL to the Android home screen, firing
  // background tasks to pull all the data required.
  static void AddShortcut(content::WebContents* web_contents,
                          const base::string16& title,
                          int launcher_larger_icon_size);

  // Adds a shortcut to the launcher.  Must be called from a WorkerPool task.
  static void AddShortcutInBackground(
      const GURL& url,
      const base::string16& title,
      ShortcutBuilder::ShortcutType shortcut_type,
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // Registers JNI hooks.
  static bool RegisterShortcutHelper(JNIEnv* env);

 private:
  DISALLOW_COPY_AND_ASSIGN(ShortcutHelper);
};

#endif  // CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
