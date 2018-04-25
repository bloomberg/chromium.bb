// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_UI_EVENT_HANDLER_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_UI_EVENT_HANDLER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

namespace ui {
class KeyEventAndroid;
}  // namespace ui

namespace content {

// Handles UI events that need Java layer access.
// Owned by |WebContentsViewAndroid|.
class ContentUiEventHandler {
 public:
  ContentUiEventHandler(JNIEnv* env,
                        const base::android::JavaRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  bool OnKeyUp(const ui::KeyEventAndroid& event);
  bool DispatchKeyEvent(const ui::KeyEventAndroid& event);

 private:
  // A weak reference to the Java ContentUiEventHandler object.
  JavaObjectWeakGlobalRef java_ref_;

  DISALLOW_COPY_AND_ASSIGN(ContentUiEventHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_UI_EVENT_HANDLER_H_
