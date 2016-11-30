// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/android/system/watcher_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "jni/WatcherImpl_jni.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/watcher.h"

namespace mojo {
namespace android {

using base::android::JavaParamRef;

namespace {

class WatcherWithMessageLoopObserver
    : public base::MessageLoop::DestructionObserver {
 public:
  WatcherWithMessageLoopObserver() {}

  ~WatcherWithMessageLoopObserver() override { StopObservingIfNecessary(); }

  jint Start(JNIEnv* env,
             const JavaParamRef<jobject>& jcaller,
             jint mojo_handle,
             jint signals) {
    if (!is_observing_) {
      is_observing_ = true;
      base::MessageLoop::current()->AddDestructionObserver(this);
    }

    java_watcher_.Reset(env, jcaller);

    auto ready_callback = base::Bind(
        &WatcherWithMessageLoopObserver::OnHandleReady, base::Unretained(this));

    MojoResult result =
        watcher_.Start(mojo::Handle(static_cast<MojoHandle>(mojo_handle)),
                       static_cast<MojoHandleSignals>(signals), ready_callback);

    if (result != MOJO_RESULT_OK) {
      StopObservingIfNecessary();
      java_watcher_.Reset();
    }

    return result;
  }

  void Cancel() {
    StopObservingIfNecessary();
    java_watcher_.Reset();
    watcher_.Cancel();
  }

 private:
  void OnHandleReady(MojoResult result) {
    DCHECK(!java_watcher_.is_null());

    base::android::ScopedJavaGlobalRef<jobject> java_watcher_preserver;
    if (result == MOJO_RESULT_CANCELLED) {
      StopObservingIfNecessary();
      java_watcher_preserver = std::move(java_watcher_);
    }

    Java_WatcherImpl_onHandleReady(
        base::android::AttachCurrentThread(),
        java_watcher_.is_null() ? java_watcher_preserver : java_watcher_,
        result);
  }

  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    StopObservingIfNecessary();
    OnHandleReady(MOJO_RESULT_ABORTED);
  }

  void StopObservingIfNecessary() {
    if (is_observing_) {
      is_observing_ = false;
      base::MessageLoop::current()->RemoveDestructionObserver(this);
    }
  }

  bool is_observing_ = false;
  Watcher watcher_;
  base::android::ScopedJavaGlobalRef<jobject> java_watcher_;

  DISALLOW_COPY_AND_ASSIGN(WatcherWithMessageLoopObserver);
};

}  // namespace

static jlong CreateWatcher(JNIEnv* env, const JavaParamRef<jobject>& jcaller) {
  return reinterpret_cast<jlong>(new WatcherWithMessageLoopObserver);
}

static jint Start(JNIEnv* env,
                  const JavaParamRef<jobject>& jcaller,
                  jlong watcher_ptr,
                  jint mojo_handle,
                  jint signals) {
  auto watcher = reinterpret_cast<WatcherWithMessageLoopObserver*>(watcher_ptr);
  return watcher->Start(env, jcaller, mojo_handle, signals);
}

static void Cancel(JNIEnv* env,
                   const JavaParamRef<jobject>& jcaller,
                   jlong watcher_ptr) {
  reinterpret_cast<WatcherWithMessageLoopObserver*>(watcher_ptr)->Cancel();
}

static void Delete(JNIEnv* env,
                   const JavaParamRef<jobject>& jcaller,
                   jlong watcher_ptr) {
  delete reinterpret_cast<WatcherWithMessageLoopObserver*>(watcher_ptr);
}

bool RegisterWatcherImpl(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace mojo
