// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_ANDROID_BLIMP_ENVIRONMENT_H_
#define BLIMP_CLIENT_APP_ANDROID_BLIMP_ENVIRONMENT_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

class PrefService;

namespace base {
class Thread;
}  // namespace base

namespace blimp {
namespace client {
class BlimpClientContext;
class BlimpClientContextDelegate;
class CompositorDependencies;
class CompositorDependenciesImpl;

// BlimpEnvironment is the core environment required to run Blimp for Android.
class BlimpEnvironment {
 public:
  BlimpEnvironment();
  ~BlimpEnvironment();
  static bool RegisterJni(JNIEnv* env);

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);

  static BlimpEnvironment* FromJavaObject(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj);

  base::android::ScopedJavaLocalRef<jobject> GetBlimpClientContext(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj);

  std::unique_ptr<CompositorDependencies> CreateCompositorDepencencies();

 private:
  friend class DelegatingCompositorDependencies;

  void DecrementOutstandingCompositorDependencies();

  // The CompositorDependencies used as the delegate for all minted
  // CompositorDependencies.
  std::unique_ptr<CompositorDependenciesImpl> compositor_dependencies_;

  // The number of outstanding CompositorDependencies. The minted
  // DelegatingCompositorDependencies will decrement this value during their
  // destruction.
  int outstanding_compositor_dependencies_ = 0;

  std::unique_ptr<base::Thread> io_thread_;

  std::unique_ptr<PrefService> pref_service_;

  std::unique_ptr<BlimpClientContextDelegate> context_delegate_;

  std::unique_ptr<BlimpClientContext> context_;

  DISALLOW_COPY_AND_ASSIGN(BlimpEnvironment);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_ANDROID_BLIMP_ENVIRONMENT_H_
