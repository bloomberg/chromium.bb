// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/status_tray_android.h"

#include "base/logging.h"
#include "jni/StatusTray_jni.h"

StatusTrayAndroid::StatusTrayAndroid() {
  java_object_.Reset(Java_StatusTray_create(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext()));
}

StatusTrayAndroid::~StatusTrayAndroid() {
}

// TODO(cramya): This is not the perfect place for this call.
// Probably makes sense to have a ScreenCaptureNotificationUIAndroid
// to make this call. Once IsCapturingVideo() and IsCapturingAudio()
// returns the correct values, this can be refactored.
StatusIcon* StatusTrayAndroid::CreatePlatformStatusIcon() {
  Java_StatusTray_createMediaCaptureStatusNotification(
      base::android::AttachCurrentThread(),
      java_object_.obj());
  return NULL;
}

StatusTray* StatusTray::Create() {
  return new StatusTrayAndroid();
}

// static
bool StatusTrayAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
