// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ANDROID_CAST_CONTENT_WINDOW_ANDROID_H_
#define CHROMECAST_BROWSER_ANDROID_CAST_CONTENT_WINDOW_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chromecast/browser/cast_content_window.h"

namespace content {
class WebContents;
}

namespace chromecast {
namespace shell {

// Android implementation of CastContentWindow, which displays WebContents in
// CastWebContentsActivity.
class CastContentWindowAndroid : public CastContentWindow {
 public:
  static bool RegisterJni(JNIEnv* env);

  ~CastContentWindowAndroid() override;

  // CastContentWindow implementation:
  void SetTransparent() override;
  void ShowWebContents(content::WebContents* web_contents,
                       CastWindowManager* window_manager) override;

  // Called through JNI.
  void OnActivityStopped(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jcaller);
  void OnKeyDown(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& jcaller,
                 int keycode);

 private:
  friend class CastContentWindow;

  // This class should only be instantiated by CastContentWindow::Create.
  explicit CastContentWindowAndroid(CastContentWindow::Delegate* delegate);

  CastContentWindow::Delegate* const delegate_;
  base::android::ScopedJavaGlobalRef<jobject> java_window_;

  DISALLOW_COPY_AND_ASSIGN(CastContentWindowAndroid);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ANDROID_CAST_CONTENT_WINDOW_ANDROID_H_
