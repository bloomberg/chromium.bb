// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/tracing_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "content/browser/tracing/trace_subscriber_stdio.h"
#include "content/public/browser/trace_controller.h"
#include "jni/TracingControllerAndroid_jni.h"

namespace content {

static jlong Init(JNIEnv* env, jobject obj) {
  TracingControllerAndroid* profiler = new TracingControllerAndroid(env, obj);
  return reinterpret_cast<intptr_t>(profiler);
}

class TracingControllerAndroid::Subscriber
    : public content::TraceSubscriberStdio {
 public:
  Subscriber(TracingControllerAndroid* profiler, const base::FilePath& path)
      : TraceSubscriberStdio(path, FILE_TYPE_ARRAY, false),
        profiler_(profiler) {}

  void set_profiler(TracingControllerAndroid* profiler) {
    CHECK(!profiler_);
    profiler_ = profiler;
  }

  virtual void OnEndTracingComplete() OVERRIDE {
    TraceSubscriberStdio::OnEndTracingComplete();
    profiler_->OnTracingStopped();
  }

 private:
  TracingControllerAndroid* profiler_;
};

TracingControllerAndroid::TracingControllerAndroid(JNIEnv* env, jobject obj)
    : weak_java_object_(env, obj) {}

TracingControllerAndroid::~TracingControllerAndroid() {}

void TracingControllerAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

bool TracingControllerAndroid::StartTracing(JNIEnv* env,
                                            jobject obj,
                                            jstring jfilename,
                                            jstring jcategories,
                                            jboolean record_continuously) {
  if (subscriber_.get()) {
    return false;
  }
  std::string filename = base::android::ConvertJavaStringToUTF8(env, jfilename);
  std::string categories =
      base::android::ConvertJavaStringToUTF8(env, jcategories);
  subscriber_.reset(new Subscriber(this, base::FilePath(filename)));
  return TraceController::GetInstance()->BeginTracing(
      subscriber_.get(),
      categories,
      record_continuously ? base::debug::TraceLog::RECORD_CONTINUOUSLY
                          : base::debug::TraceLog::RECORD_UNTIL_FULL);
}

void TracingControllerAndroid::StopTracing(JNIEnv* env, jobject obj) {
  if (!subscriber_.get()) {
    return;
  }
  TraceController* controller = TraceController::GetInstance();
  if (!controller->EndTracingAsync(subscriber_.get())) {
    LOG(ERROR) << "EndTracingAsync failed, forcing an immediate stop";
    controller->CancelSubscriber(subscriber_.get());
    OnTracingStopped();
  }
}

void TracingControllerAndroid::OnTracingStopped() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_object_.get(env);
  if (obj.obj()) {
    Java_TracingControllerAndroid_onTracingStopped(env, obj.obj());
  }
  subscriber_.reset();
}

static jstring GetDefaultCategories(JNIEnv* env, jobject obj) {
  return base::android::ConvertUTF8ToJavaString(env,
      base::debug::CategoryFilter::kDefaultCategoryFilterString).Release();
}

bool RegisterTracingControllerAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
