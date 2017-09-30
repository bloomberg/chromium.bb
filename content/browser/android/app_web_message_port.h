// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_H_
#define CONTENT_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_H_

#include "base/android/jni_weak_ref.h"
#include "third_party/WebKit/common/message_port/message_port_channel.h"

namespace content {

class AppWebMessagePort {
 public:
  static void CreateAndBindToJavaObject(
      JNIEnv* env,
      mojo::ScopedMessagePipeHandle handle,
      const base::android::JavaRef<jobject>& jobject);

  static std::vector<blink::MessagePortChannel> UnwrapJavaArray(
      JNIEnv* env,
      const base::android::JavaRef<jobjectArray>& jports);

  // Methods called from Java.
  void CloseMessagePort(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  void PostMessage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jmessage,
      const base::android::JavaParamRef<jobjectArray>& jports);
  jboolean DispatchNextMessage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  void StartReceivingMessages(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

 private:
  explicit AppWebMessagePort(
      JNIEnv* env,
      mojo::ScopedMessagePipeHandle handle,
      const base::android::JavaRef<jobject>& jobject);
  ~AppWebMessagePort();

  void OnMessagesAvailable();

  blink::MessagePortChannel channel_;
  JavaObjectWeakGlobalRef java_ref_;

  DISALLOW_COPY_AND_ASSIGN(AppWebMessagePort);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_H_
