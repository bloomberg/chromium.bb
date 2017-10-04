// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/app_web_message_port.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "content/browser/android/string_message_codec.h"
#include "jni/AppWebMessagePort_jni.h"

using blink::MessagePortChannel;

namespace content {

// static
void AppWebMessagePort::CreateAndBindToJavaObject(
    JNIEnv* env,
    mojo::ScopedMessagePipeHandle handle,
    const base::android::JavaRef<jobject>& jobject) {
  AppWebMessagePort* instance =
      new AppWebMessagePort(env, std::move(handle), jobject);
  Java_AppWebMessagePort_setNativeAppWebMessagePort(
      env, jobject, reinterpret_cast<jlong>(instance));
}

// static
std::vector<MessagePortChannel> AppWebMessagePort::UnwrapJavaArray(
    JNIEnv* env,
    const base::android::JavaRef<jobjectArray>& jports) {
  std::vector<MessagePortChannel> channels;
  if (!jports.is_null()) {
    jsize num_ports = env->GetArrayLength(jports.obj());
    for (jsize i = 0; i < num_ports; ++i) {
      base::android::ScopedJavaLocalRef<jobject> jport(
          env, env->GetObjectArrayElement(jports.obj(), i));
      jlong native_port =
          Java_AppWebMessagePort_releaseNativePortForTransfer(env, jport);
      // Uninitialized ports should be trapper earlier at the Java layer.
      DCHECK(native_port != -1);
      AppWebMessagePort* instance =
          reinterpret_cast<AppWebMessagePort*>(native_port);
      channels.push_back(instance->channel_);  // Transfers ownership.
      delete instance;
    }
  }
  return channels;
}

void AppWebMessagePort::CloseMessagePort(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  // Explicitly reset the port here to ensure that OnMessagesAvailable has
  // finished before we destroy this.
  channel_ = MessagePortChannel();

  delete this;
}

void AppWebMessagePort::PostMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jmessage,
    const base::android::JavaParamRef<jobjectArray>& jports) {
  std::vector<uint8_t> encoded_message =
      EncodeStringMessage(base::android::ConvertJavaStringToUTF16(jmessage));
  channel_.PostMessage(encoded_message.data(), encoded_message.size(),
                       UnwrapJavaArray(env, jports));
}

jboolean AppWebMessagePort::DispatchNextMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;

  jmethodID app_web_message_port_constructor =
      base::android::MethodID::Get<base::android::MethodID::TYPE_INSTANCE>(
          env, org_chromium_content_browser_AppWebMessagePort_clazz(env),
          "<init>", "()V");

  std::vector<uint8_t> encoded_message;
  std::vector<MessagePortChannel> channels;
  if (!channel_.GetMessage(&encoded_message, &channels))
    return false;

  base::string16 message;
  if (!DecodeStringMessage(encoded_message, &message))
    return false;

  base::android::ScopedJavaLocalRef<jstring> jmessage =
      base::android::ConvertUTF16ToJavaString(env, message);

  base::android::ScopedJavaLocalRef<jobjectArray> jports;
  if (channels.size() > 0) {
    jports = base::android::ScopedJavaLocalRef<jobjectArray>(
        env, env->NewObjectArray(
                 channels.size(),
                 org_chromium_content_browser_AppWebMessagePort_clazz(env),
                 nullptr));

    // Instantiate the Java and C++ wrappers for the transferred ports.
    for (size_t i = 0; i < channels.size(); ++i) {
      base::android::ScopedJavaLocalRef<jobject> jport(
          env, env->NewObject(
                   org_chromium_content_browser_AppWebMessagePort_clazz(env),
                   app_web_message_port_constructor));
      CreateAndBindToJavaObject(env, channels[i].ReleaseHandle(), jport);

      env->SetObjectArrayElement(jports.obj(), i, jport.obj());
    }
  }

  Java_AppWebMessagePort_onReceivedMessage(env, obj, jmessage, jports);
  return true;
}

void AppWebMessagePort::StartReceivingMessages(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  channel_.SetCallback(base::Bind(&AppWebMessagePort::OnMessagesAvailable,
                                  base::Unretained(this)));
}

AppWebMessagePort::AppWebMessagePort(
    JNIEnv* env,
    mojo::ScopedMessagePipeHandle handle,
    const base::android::JavaRef<jobject>& jobject)
    : channel_(std::move(handle)), java_ref_(env, jobject) {}

AppWebMessagePort::~AppWebMessagePort() {
}

void AppWebMessagePort::OnMessagesAvailable() {
  // Called on a background thread.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AppWebMessagePort_onMessagesAvailable(env, obj);
}

void InitializeAppWebMessagePortPair(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobjectArray>& ports) {
  DCHECK_EQ(2, env->GetArrayLength(ports.obj()));

  mojo::MessagePipe pipe;

  base::android::ScopedJavaLocalRef<jobject> jport0(
      env, env->GetObjectArrayElement(ports.obj(), 0));
  AppWebMessagePort::CreateAndBindToJavaObject(
      env, std::move(pipe.handle0), jport0);

  base::android::ScopedJavaLocalRef<jobject> jport1(
      env, env->GetObjectArrayElement(ports.obj(), 1));
  AppWebMessagePort::CreateAndBindToJavaObject(
      env, std::move(pipe.handle1), jport1);
}

}  // namespace content
