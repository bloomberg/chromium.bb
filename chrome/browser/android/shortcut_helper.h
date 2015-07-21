// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
#define CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/basictypes.h"
#include "chrome/browser/android/shortcut_data_fetcher.h"
#include "chrome/browser/android/shortcut_info.h"
#include "content/public/common/manifest.h"

namespace content {
class WebContents;
}  // namespace content

namespace IPC {
class Message;
}

class GURL;

// ShortcutHelper is the C++ counterpart of org.chromium.chrome.browser's
// ShortcutHelper in Java. The object is owned by the Java object. It is created
// from there via a JNI (Initialize) call and MUST BE DESTROYED via Destroy().
class ShortcutHelper : public ShortcutDataFetcher::Observer {
 public:
  ShortcutHelper(JNIEnv* env,
                 jobject obj,
                 content::WebContents* web_contents);

  // Called by the Java counterpart to destroy its native half.
  void Destroy(JNIEnv* env, jobject obj);

  // Registers JNI hooks.
  static bool RegisterShortcutHelper(JNIEnv* env);

  // Adds a shortcut to the current URL to the Android home screen.
  void AddShortcut(JNIEnv* env, jobject obj, jstring title);

  // Adds a shortcut to the launcher using a SkBitmap.
  // Must be called on the IO thread.
  static void AddShortcutInBackgroundWithSkBitmap(const ShortcutInfo& info,
                                                  const SkBitmap& icon_bitmap);

  // ShortcutDataFetcher::Observer
  void OnUserTitleAvailable(const base::string16& user_title) override;
  void OnDataAvailable(const ShortcutInfo& info, const SkBitmap& icon) override;
  SkBitmap FinalizeLauncherIcon(const SkBitmap& icon, const GURL& url) override;

 private:
  virtual ~ShortcutHelper();

  // Called only when the ShortcutDataFetcher has retrieved all of the
  // data needed to add the shortcut.
  void AddShortcut(const ShortcutInfo& info, const SkBitmap& icon);

  void RecordAddToHomescreen();

  // Points to the Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // Whether the user has requested that a shortcut be added while a fetch was
  // in progress.
  bool add_shortcut_pending_;

  // Fetches data required to add a shortcut.
  scoped_refptr<ShortcutDataFetcher> data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutHelper);
};

#endif  // CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
