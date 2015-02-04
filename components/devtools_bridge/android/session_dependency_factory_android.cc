// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_bridge/android/session_dependency_factory_android.h"

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "components/devtools_bridge/abstract_data_channel.h"
#include "components/devtools_bridge/abstract_peer_connection.h"
#include "components/devtools_bridge/rtc_configuration.h"
#include "components/devtools_bridge/socket_tunnel_server.h"
#include "jni/SessionDependencyFactoryNative_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

namespace devtools_bridge {
namespace android {

namespace {

/**
 * Wraps Java observer and adapts it to native delegate. Chromium code normally
 * leaves local java references for automatic disposing. It doesn't happen here
 * (because calls originated from native thread). For instance, instead of
 *
 * ConvertUTF8ToJavaString(env, ...).Release()
 *
 * please use ConvertUTF8ToJavaString(env, ...).obj() or ScopedJavaLocalFrame.
 */
class PeerConnectionDelegateImpl
    : public AbstractPeerConnection::Delegate {
 public:
  PeerConnectionDelegateImpl(JNIEnv* env, jobject java_object) {
    java_object_.Reset(env, java_object);
    connected_ = false;
  }

  void OnIceConnectionChange(bool connected) override {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyIceConnectionChange(
        env, java_object_.obj(), connected);
  }

  void OnIceCandidate(const std::string& sdp_mid,
                      int sdp_mline_index,
                      const std::string& sdp) override {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyIceCandidate(
        env, java_object_.obj(),
        ConvertUTF8ToJavaString(env, sdp_mid).obj(),
        sdp_mline_index, ConvertUTF8ToJavaString(env, sdp).obj());
  }

  void NotifyLocalOfferCreatedAndSetSet(const std::string& description) {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyLocalOfferCreatedAndSetSet(
        env, java_object_.obj(),
        ConvertUTF8ToJavaString(env, description).obj());
  }

  void OnLocalOfferCreatedAndSetSet(const std::string& description) override {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyLocalOfferCreatedAndSetSet(
        env, java_object_.obj(),
        ConvertUTF8ToJavaString(env, description).obj());
  }

  void OnLocalAnswerCreatedAndSetSet(const std::string& description) override {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyLocalAnswerCreatedAndSetSet(
        env, java_object_.obj(),
        ConvertUTF8ToJavaString(env, description).obj());
  }

  void OnRemoteDescriptionSet() override {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyRemoteDescriptionSet(
        env, java_object_.obj());
  }

  void OnFailure(const std::string& description) override {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyConnectionFailure(
        env, java_object_.obj(),
        ConvertUTF8ToJavaString(env, description).obj());
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  bool connected_;
};

class DataChannelObserverImpl : public AbstractDataChannel::Observer {
 public:
  DataChannelObserverImpl(JNIEnv* env, jobject java_object) {
    java_object_.Reset(env, java_object);
  }

  void OnOpen() override {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyChannelOpen(
        env, java_object_.obj());
  }

  void OnClose() override {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyChannelClose(
        env, java_object_.obj());
  }

  void OnMessage(const void* data, size_t length) override {
    JNIEnv* env = AttachCurrentThread();

    ScopedJavaLocalRef<jobject> byte_buffer(
        env, env->NewDirectByteBuffer(const_cast<void*>(data), length));

    Java_SessionDependencyFactoryNative_notifyMessage(
        env, java_object_.obj(), byte_buffer.obj());
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

static void CleanupOnSignalingThread() {
  // Called on signaling thread when SessionDependencyFactory is destroying.
  base::android::DetachFromVM();
}

}  // namespace

// SessionDependencyFactoryNative

SessionDependencyFactoryAndroid::SessionDependencyFactoryAndroid()
    : impl_(SessionDependencyFactory::CreateInstance(
          base::Bind(&CleanupOnSignalingThread))) {
}

SessionDependencyFactoryAndroid::~SessionDependencyFactoryAndroid() {
}

// static
bool SessionDependencyFactoryAndroid::RegisterNatives(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

scoped_ptr<AbstractPeerConnection>
SessionDependencyFactoryAndroid::CreatePeerConnection(
    scoped_ptr<RTCConfiguration> config,
    scoped_ptr<AbstractPeerConnection::Delegate> delegate) {
  return impl_->CreatePeerConnection(config.Pass(), delegate.Pass());
}

scoped_refptr<base::TaskRunner>
SessionDependencyFactoryAndroid::signaling_thread_task_runner() {
  return impl_->signaling_thread_task_runner();
}

scoped_refptr<base::TaskRunner>
SessionDependencyFactoryAndroid::io_thread_task_runner() {
  return impl_->io_thread_task_runner();
}

// JNI generated methods

static jlong CreateFactory(JNIEnv* env, jclass jcaller) {
  return reinterpret_cast<jlong>(new SessionDependencyFactoryAndroid());
}

static void DestroyFactory(JNIEnv* env, jclass jcaller, jlong factory_ptr) {
  delete reinterpret_cast<SessionDependencyFactoryAndroid*>(factory_ptr);
}

static jlong CreateConfig(JNIEnv* env, jclass jcaller) {
  return reinterpret_cast<jlong>(
      RTCConfiguration::CreateInstance().release());
}

static void AddIceServer(
    JNIEnv* env, jclass jcaller, jlong config_ptr,
    jstring uri, jstring username, jstring credential) {
  reinterpret_cast<RTCConfiguration*>(config_ptr)->AddIceServer(
      ConvertJavaStringToUTF8(env, uri),
      ConvertJavaStringToUTF8(env, username),
      ConvertJavaStringToUTF8(env, credential));
}

static jlong CreatePeerConnection(
    JNIEnv* env, jclass jcaller,
    jlong factory_ptr, jlong config_ptr, jobject observer) {
  auto factory =
      reinterpret_cast<SessionDependencyFactoryAndroid*>(factory_ptr);
  auto config = reinterpret_cast<RTCConfiguration*>(config_ptr);

  auto delegate = new PeerConnectionDelegateImpl(env, observer);

  return reinterpret_cast<jlong>(factory->CreatePeerConnection(
         make_scoped_ptr(config), make_scoped_ptr(delegate)).release());
}

static void DestroyPeerConnection(
    JNIEnv* env, jclass jcaller, jlong connection_ptr) {
  delete reinterpret_cast<AbstractPeerConnection*>(connection_ptr);
}

static void CreateAndSetLocalOffer(
    JNIEnv* env, jclass jcaller, jlong connection_ptr) {
  reinterpret_cast<AbstractPeerConnection*>(
      connection_ptr)->CreateAndSetLocalOffer();
}

static void CreateAndSetLocalAnswer(
    JNIEnv* env, jclass jcaller, jlong connection_ptr) {
  reinterpret_cast<AbstractPeerConnection*>(
      connection_ptr)->CreateAndSetLocalAnswer();
}

static void SetRemoteOffer(
    JNIEnv* env, jclass jcaller, jlong connection_ptr, jstring description) {
  reinterpret_cast<AbstractPeerConnection*>(connection_ptr)->SetRemoteOffer(
      ConvertJavaStringToUTF8(env, description));
}

static void SetRemoteAnswer(
    JNIEnv* env, jclass jcaller, jlong connection_ptr, jstring description) {
  reinterpret_cast<AbstractPeerConnection*>(connection_ptr)->SetRemoteAnswer(
      ConvertJavaStringToUTF8(env, description));
}

static void AddIceCandidate(
    JNIEnv* env, jclass jcaller,
    jlong connection_ptr, jstring sdp_mid, jint sdp_mline_index, jstring sdp) {
  reinterpret_cast<AbstractPeerConnection*>(connection_ptr)->AddIceCandidate(
      ConvertJavaStringToUTF8(env, sdp_mid),
      sdp_mline_index,
      ConvertJavaStringToUTF8(env, sdp));
}

static jlong CreateDataChannel(
    JNIEnv* env, jclass jcaller, jlong connection_ptr, jint channel_id) {
  return reinterpret_cast<jlong>(
      reinterpret_cast<AbstractPeerConnection*>(
          connection_ptr)->CreateDataChannel(channel_id).release());
}

static void DestroyDataChannel(
    JNIEnv* env, jclass jcaller, jlong channel_ptr) {
  delete reinterpret_cast<AbstractDataChannel*>(channel_ptr);
}

static void RegisterDataChannelObserver(
    JNIEnv* env, jclass jcaller, jlong channel_ptr, jobject observer) {
  reinterpret_cast<AbstractDataChannel*>(channel_ptr)->RegisterObserver(
      make_scoped_ptr(new DataChannelObserverImpl(env, observer)));
}

static void UnregisterDataChannelObserver(
    JNIEnv* env, jclass jcaller, jlong channel_ptr) {
  reinterpret_cast<AbstractDataChannel*>(channel_ptr)->UnregisterObserver();
}

static void SendBinaryMessage(
    JNIEnv* env, jclass jcaller, jlong channel_ptr, jobject message,
    jint size) {
  DCHECK(size > 0);
  reinterpret_cast<AbstractDataChannel*>(channel_ptr)->SendBinaryMessage(
      env->GetDirectBufferAddress(message), size);
}

static void SendTextMessage(
    JNIEnv* env, jclass jcaller, jlong channel_ptr, jobject message,
    jint size) {
  DCHECK(size > 0);
  reinterpret_cast<AbstractDataChannel*>(channel_ptr)->SendTextMessage(
      env->GetDirectBufferAddress(message), size);
}

static void CloseDataChannel(JNIEnv* env, jclass jcaller, jlong channel_ptr) {
  reinterpret_cast<AbstractDataChannel*>(channel_ptr)->Close();
}

static jlong CreateSocketTunnelServer(
    JNIEnv* env, jclass jcaller, jlong factory_ptr, jlong channel_ptr,
    jstring socket_name) {
  return reinterpret_cast<jlong>(
      new SocketTunnelServer(
          reinterpret_cast<SessionDependencyFactory*>(factory_ptr),
          reinterpret_cast<AbstractDataChannel*>(channel_ptr),
          ConvertJavaStringToUTF8(env, socket_name)));
}

static void DestroySocketTunnelServer(
    JNIEnv* env, jclass jcaller, jlong tunnel_ptr) {
  delete reinterpret_cast<SocketTunnelServer*>(tunnel_ptr);
}

}  // namespace android
}  // namespace devtools_bridge
