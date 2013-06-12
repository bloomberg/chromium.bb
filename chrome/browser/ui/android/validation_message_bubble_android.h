// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_VALIDATION_MESSAGE_BUBBLE_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_VALIDATION_MESSAGE_BUBBLE_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/validation_message_bubble.h"

namespace content {
class RenderWidgetHost;
}

namespace gfx {
class Rect;
}

// An implementation of ValidationMessageBubble for Android. This class is a
// bridge to a Java implementation.
class ValidationMessageBubbleAndroid : public chrome::ValidationMessageBubble {
 public:
  ValidationMessageBubbleAndroid(content::RenderWidgetHost* widget_host,
                                 const gfx::Rect& anchor_in_screen,
                                 const string16& main_text,
                                 const string16& sub_text);
  virtual ~ValidationMessageBubbleAndroid();
  virtual void SetPositionRelativeToAnchor(
      content::RenderWidgetHost* widget_host,
      const gfx::Rect& anchor_in_screen) OVERRIDE;

  static bool Register(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_validation_message_bubble_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_VALIDATION_MESSAGE_BUBBLE_ANDROID_H_
