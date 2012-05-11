// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/data_fetcher_impl_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "content/browser/device_orientation/orientation.h"
#include "jni/device_orientation_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::GetClass;
using base::android::GetMethodID;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace device_orientation {

namespace {

// This should match ProviderImpl::kDesiredSamplingIntervalMs.
// TODO(husky): Make that constant public so we can use it directly.
const int kPeriodInMilliseconds = 100;

base::LazyInstance<ScopedJavaGlobalRef<jobject> >
     g_jni_obj = LAZY_INSTANCE_INITIALIZER;

}  // namespace

DataFetcherImplAndroid::DataFetcherImplAndroid() {
}

void DataFetcherImplAndroid::Init(JNIEnv* env) {
  bool result = RegisterNativesImpl(env);
  DCHECK(result);

  g_jni_obj.Get().Reset(Java_DeviceOrientation_create(env));
}

DataFetcher* DataFetcherImplAndroid::Create() {
  scoped_ptr<DataFetcherImplAndroid> fetcher(new DataFetcherImplAndroid);
  if (fetcher->Start(kPeriodInMilliseconds))
    return fetcher.release();

  LOG(ERROR) << "DataFetcherImplAndroid::Start failed!";
  return NULL;
}

DataFetcherImplAndroid::~DataFetcherImplAndroid() {
  Stop();
}

bool DataFetcherImplAndroid::GetOrientation(Orientation* orientation) {
  // Do we have a new orientation value? (It's safe to do this outside the lock
  // because we only skip the lock if the value is null. We always enter the
  // lock if we're going to make use of the new value.)
  if (next_orientation_.get()) {
    base::AutoLock autolock(next_orientation_lock_);
    next_orientation_.swap(current_orientation_);
  }
  if (current_orientation_.get())
    *orientation = *current_orientation_;
  return true;
}

void DataFetcherImplAndroid::GotOrientation(
    JNIEnv*, jobject, double alpha, double beta, double gamma) {
  base::AutoLock autolock(next_orientation_lock_);
  next_orientation_.reset(
      new Orientation(true, alpha, true, beta, true, gamma, true, true));
}

bool DataFetcherImplAndroid::Start(int rate_in_milliseconds) {
  DCHECK(!g_jni_obj.Get().is_null());
  return Java_DeviceOrientation_start(AttachCurrentThread(),
                                      g_jni_obj.Get().obj(),
                                      reinterpret_cast<jint>(this),
                                      rate_in_milliseconds);
}

void DataFetcherImplAndroid::Stop() {
  DCHECK(!g_jni_obj.Get().is_null());
  Java_DeviceOrientation_stop(AttachCurrentThread(), g_jni_obj.Get().obj());
}

}  // namespace device_orientation
