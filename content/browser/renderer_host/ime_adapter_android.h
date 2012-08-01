// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_IME_ADAPTER_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_IME_ADAPTER_ANDROID_H_

#include <jni.h>

namespace content {

class RenderWidgetHostViewAndroid;
struct NativeWebKeyboardEvent;

// This class is in charge of dispatching key events from the java side
// and forward to renderer along with input method results via
// corresponding host view.
class ImeAdapterAndroid {
 public:
  explicit ImeAdapterAndroid(RenderWidgetHostViewAndroid* rwhva);
  ~ImeAdapterAndroid();

  static bool RegisterImeAdapter(JNIEnv* env);

  // Called from java -> native
  // The java side is responsible to translate android KeyEvent various enums
  // and values into the corresponding WebKit::WebInputEvent.
  void Unselect(JNIEnv*, jobject);
  void SelectAll(JNIEnv*, jobject);
  void Cut(JNIEnv*, jobject);
  void Copy(JNIEnv*, jobject);
  void Paste(JNIEnv*, jobject);

 private:
  RenderWidgetHostViewAndroid* rwhva_;
  jobject java_ime_adapter_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_IME_ADAPTER_ANDROID_H_
