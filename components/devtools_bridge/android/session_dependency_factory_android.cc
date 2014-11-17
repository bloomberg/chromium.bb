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
#include "jni/SessionDependencyFactoryNative_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

namespace devtools_bridge {
namespace android {

namespace {

class PeerConnectionDelegateImpl
    : public AbstractPeerConnection::Delegate {
 public:
  PeerConnectionDelegateImpl(JNIEnv* env, jobject java_object) {
    java_object_.Reset(env, java_object);
    connected_ = false;
  }

  virtual void OnIceConnectionChange(bool connected) override {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyIceConnectionChange(
        env, java_object_.obj(), connected);
  }

  virtual void OnIceCandidate(
      const std::string& sdp_mid, int sdp_mline_index, const std::string& sdp)
      override {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyIceCandidate(
        env, java_object_.obj(),
        ConvertUTF8ToJavaString(env, sdp_mid).Release(),
        sdp_mline_index, ConvertUTF8ToJavaString(env, sdp).Release());
  }

  void NotifyLocalOfferCreatedAndSetSet(const std::string& description) {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyLocalOfferCreatedAndSetSet(
        env, java_object_.obj(),
        ConvertUTF8ToJavaString(env, description).Release());
  }

  virtual void OnLocalOfferCreatedAndSetSet(const std::string& description)
      override {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyLocalOfferCreatedAndSetSet(
        env, java_object_.obj(),
        ConvertUTF8ToJavaString(env, description).Release());
  }

  virtual void OnLocalAnswerCreatedAndSetSet(const std::string& description)
      override {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyLocalAnswerCreatedAndSetSet(
        env, java_object_.obj(),
        ConvertUTF8ToJavaString(env, description).Release());
  }

  virtual void OnRemoteDescriptionSet() override {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyRemoteDescriptionSet(
        env, java_object_.obj());
  }

  virtual void OnFailure(const std::string& description) override {
    JNIEnv* env = AttachCurrentThread();
    Java_SessionDependencyFactoryNative_notifyConnectionFailure(
        env, java_object_.obj(),
        ConvertUTF8ToJavaString(env, description).Release());
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  bool connected_;
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
void SessionDependencyFactoryAndroid::RegisterNatives(JNIEnv* env) {
  RegisterNativesImpl(env);
}

scoped_ptr<AbstractPeerConnection>
SessionDependencyFactoryAndroid::CreatePeerConnection(
    scoped_ptr<RTCConfiguration> config,
    scoped_ptr<AbstractPeerConnection::Delegate> delegate) {
  return impl_->CreatePeerConnection(config.Pass(), delegate.Pass());
}

// JNI generated methods

static jlong CreateFactory(JNIEnv* env, jclass jcaller) {
  return reinterpret_cast<jlong>(new SessionDependencyFactoryAndroid());
}

static void DestroyFactory(JNIEnv* env, jclass jcaller, jlong factoryPtr) {
  delete reinterpret_cast<SessionDependencyFactoryAndroid*>(factoryPtr);
}

static jlong CreateConfig(JNIEnv* env, jclass jcaller) {
  return reinterpret_cast<jlong>(
      RTCConfiguration::CreateInstance().release());
}

static void AddIceServer(
    JNIEnv* env, jclass jcaller, jlong configPtr,
    jstring uri, jstring username, jstring credential) {
  reinterpret_cast<RTCConfiguration*>(configPtr)->AddIceServer(
      ConvertJavaStringToUTF8(env, uri),
      ConvertJavaStringToUTF8(env, username),
      ConvertJavaStringToUTF8(env, credential));
}

static jlong CreatePeerConnection(
    JNIEnv* env, jclass jcaller,
    jlong factoryPtr, jlong configPtr, jobject observer) {
  auto factory =
      reinterpret_cast<SessionDependencyFactoryAndroid*>(factoryPtr);
  auto config = reinterpret_cast<RTCConfiguration*>(configPtr);

  auto delegate = new PeerConnectionDelegateImpl(env, observer);

  return reinterpret_cast<jlong>(factory->CreatePeerConnection(
         make_scoped_ptr(config), make_scoped_ptr(delegate)).release());
}

static void DestroyPeerConnection(
    JNIEnv* env, jclass jcaller, jlong connectionPtr) {
  delete reinterpret_cast<AbstractPeerConnection*>(connectionPtr);
}

static void CreateAndSetLocalOffer(
    JNIEnv* env, jclass jcaller, jlong connectionPtr) {
  reinterpret_cast<AbstractPeerConnection*>(
      connectionPtr)->CreateAndSetLocalOffer();
}

static void CreateAndSetLocalAnswer(
    JNIEnv* env, jclass jcaller, jlong connectionPtr) {
  reinterpret_cast<AbstractPeerConnection*>(
      connectionPtr)->CreateAndSetLocalAnswer();
}

static void SetRemoteOffer(
    JNIEnv* env, jclass jcaller, jlong connectionPtr, jstring description) {
  reinterpret_cast<AbstractPeerConnection*>(connectionPtr)->SetRemoteOffer(
      ConvertJavaStringToUTF8(env, description));
}

static void SetRemoteAnswer(
    JNIEnv* env, jclass jcaller, jlong connectionPtr, jstring description) {
  reinterpret_cast<AbstractPeerConnection*>(connectionPtr)->SetRemoteAnswer(
      ConvertJavaStringToUTF8(env, description));
}

static void AddIceCandidate(
    JNIEnv* env, jclass jcaller,
    jlong connectionPtr, jstring sdpMid, jint sdpMLineIndex, jstring sdp) {
  reinterpret_cast<AbstractPeerConnection*>(connectionPtr)->AddIceCandidate(
      ConvertJavaStringToUTF8(env, sdpMid),
      sdpMLineIndex,
      ConvertJavaStringToUTF8(env, sdp));
}

static jlong CreateDataChannel(
    JNIEnv* env, jclass jcaller, jlong connectionPtr, jint channelId) {
  return reinterpret_cast<jlong>(
      reinterpret_cast<AbstractPeerConnection*>(
          connectionPtr)->CreateDataChannel(channelId).release());
}

static void DestroyDataChannel(
    JNIEnv* env, jclass jcaller, jlong channelPtr) {
  delete reinterpret_cast<AbstractDataChannel*>(channelPtr);
}

}  // namespace android
}  // namespace devtools_bridge
