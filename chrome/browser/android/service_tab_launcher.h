// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SERVICE_TAB_LAUNCHER_H_
#define CHROME_BROWSER_ANDROID_SERVICE_TAB_LAUNCHER_H_

#include "base/android/jni_android.h"
#include "base/callback_forward.h"
#include "base/memory/singleton.h"

namespace content {
class BrowserContext;
struct OpenURLParams;
class WebContents;
}

namespace chrome {
namespace android {

// Launcher for creating new tabs on Android from a background service, where
// there may not necessarily be an Activity or a tab model at all.
class ServiceTabLauncher {
 public:
  // Returns the singleton instance of the service tab launcher.
  static ServiceTabLauncher* GetInstance();

  // Launches a new tab when we're in a Service rather than in an Activity.
  // |callback| will be invoked with the resulting content::WebContents* when
  // the tab is avialable. This method must only be called from the UI thread.
  void LaunchTab(content::BrowserContext* browser_context,
                 const content::OpenURLParams& params,
                 const base::Callback<void(content::WebContents*)>& callback);

  static bool RegisterServiceTabLauncher(JNIEnv* env);

 private:
  friend struct DefaultSingletonTraits<ServiceTabLauncher>;

  ServiceTabLauncher();
  ~ServiceTabLauncher();

  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(ServiceTabLauncher);
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_SERVICE_TAB_LAUNCHER_H_
