// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
#define CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/common/web_application_info.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/manifest.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"

namespace content {
class WebContents;
}  // namespace content

namespace IPC {
class Message;
}

class GURL;

// ShortcutHelper is the C++ counterpart of org.chromium.chrome.browser's
// ShortcutHelper in Java. The object is owned by the Java object. It is created
// from there via a JNI (Initialize) call and can be destroyed from Java too
// using TearDown. When the Java implementations calls AddShortcut, it gives up
// the ownership of the object. The instance will then destroy itself when done.
class ShortcutHelper : public content::WebContentsObserver {
 public:
  ShortcutHelper(JNIEnv* env,
                 jobject obj,
                 content::WebContents* web_contents);

  // Initialize the helper by requesting the information about the page to the
  // renderer process. The initialization is asynchronous and
  // OnDidRetrieveWebappInformation is expected to be called when finished.
  void Initialize();

  // Called by the Java counter part to let the object knows that it can destroy
  // itself.
  void TearDown(JNIEnv* env, jobject obj);

  // IPC message received when the initialization is finished.
  void OnDidGetWebApplicationInfo(const WebApplicationInfo& web_app_info);

  // Callback run when the Manifest is ready to be used.
  void OnDidGetManifest(const content::Manifest& manifest);

  // Adds a shortcut to the current URL to the Android home screen.
  void AddShortcut(JNIEnv* env,
                   jobject obj,
                   jstring title,
                   jint launcher_large_icon_size);

  void FinishAddingShortcut(
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // WebContentsObserver
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;

  // Adds a shortcut to the launcher.  Must be called from a WorkerPool task.
  static void AddShortcutInBackground(
      const GURL& url,
      const base::string16& title,
      content::Manifest::DisplayMode display,
      const favicon_base::FaviconRawBitmapResult& bitmap_result,
      blink::WebScreenOrientationLockType orientation);

  // Registers JNI hooks.
  static bool RegisterShortcutHelper(JNIEnv* env);

 private:
  virtual ~ShortcutHelper();

  void Destroy();

  JavaObjectWeakGlobalRef java_ref_;

  GURL url_;
  base::string16 title_;
  int launcher_large_icon_size_;
  content::Manifest::DisplayMode display_;
  favicon_base::FaviconRawBitmapResult icon_;
  base::CancelableTaskTracker cancelable_task_tracker_;
  blink::WebScreenOrientationLockType orientation_;

  base::WeakPtrFactory<ShortcutHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutHelper);
};

#endif  // CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
