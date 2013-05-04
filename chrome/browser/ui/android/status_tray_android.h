// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_STATUS_TRAY_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_STATUS_TRAY_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "chrome/browser/status_icons/status_tray.h"

// Android specific implementation of StatusTray.
class StatusTrayAndroid : public StatusTray {
 public:
  StatusTrayAndroid();

  static bool Register(JNIEnv* env);

 protected:
  // Overridden from StatusTray:
  virtual StatusIcon* CreatePlatformStatusIcon() OVERRIDE;

 private:
  virtual ~StatusTrayAndroid();

  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(StatusTrayAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_STATUS_TRAY_ANDROID_H_
