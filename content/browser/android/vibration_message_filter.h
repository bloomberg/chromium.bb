// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_VIBRATION_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_ANDROID_VIBRATION_MESSAGE_FILTER_H_

#include "base/android/jni_android.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

class VibrationMessageFilter : public BrowserMessageFilter {
 public:
  VibrationMessageFilter();

  static bool Register(JNIEnv* env);

 private:
  virtual ~VibrationMessageFilter();

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  void OnVibrate(int64 milliseconds);
  void OnCancelVibration();

  base::android::ScopedJavaGlobalRef<jobject> j_vibration_message_filter_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_VIBRATION_MESSAGE_FILTER_H_
