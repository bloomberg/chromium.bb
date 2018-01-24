// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_tracing_controller.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task_scheduler/post_task.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"

#include "jni/AwTracingController_jni.h"

using base::android::JavaParamRef;

namespace {

base::android::ScopedJavaLocalRef<jbyteArray> StringToJavaBytes(
    JNIEnv* env,
    const std::string& str) {
  return base::android::ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

class AwTraceDataEndpoint
    : public content::TracingController::TraceDataEndpoint {
 public:
  typedef base::Callback<void(std::unique_ptr<std::string>)>
      ReceivedChunkCallback;
  typedef base::Callback<void(std::unique_ptr<const base::DictionaryValue>)>
      CompletedCallback;

  static scoped_refptr<content::TracingController::TraceDataEndpoint> Create(
      ReceivedChunkCallback received_chunk_callback,
      CompletedCallback completed_callback) {
    return new AwTraceDataEndpoint(received_chunk_callback, completed_callback);
  }

  void ReceiveTraceFinalContents(
      std::unique_ptr<const base::DictionaryValue> metadata) override {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(completed_callback_, base::Passed(std::move(metadata))));
  }

  void ReceiveTraceChunk(std::unique_ptr<std::string> chunk) override {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(received_chunk_callback_, base::Passed(std::move(chunk))));
  }

  explicit AwTraceDataEndpoint(ReceivedChunkCallback received_chunk_callback,
                               CompletedCallback completed_callback)
      : received_chunk_callback_(received_chunk_callback),
        completed_callback_(completed_callback) {}

 private:
  ~AwTraceDataEndpoint() override {}

  ReceivedChunkCallback received_chunk_callback_;
  CompletedCallback completed_callback_;

  DISALLOW_COPY_AND_ASSIGN(AwTraceDataEndpoint);
};

}  // namespace

namespace android_webview {

static jlong JNI_AwTracingController_Init(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  AwTracingController* controller = new AwTracingController(env, obj);
  return reinterpret_cast<intptr_t>(controller);
}

AwTracingController::AwTracingController(JNIEnv* env, jobject obj)
    : weak_java_object_(env, obj), weak_factory_(this) {}

AwTracingController::~AwTracingController() {}

bool AwTracingController::Start(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                const JavaParamRef<jstring>& jcategories,
                                jint jmode) {
  std::string categories =
      base::android::ConvertJavaStringToUTF8(env, jcategories);
  base::trace_event::TraceConfig trace_config(
      categories, static_cast<base::trace_event::TraceRecordMode>(jmode));
  // Required for filtering out potential PII.
  trace_config.EnableArgumentFilter();
  return content::TracingController::GetInstance()->StartTracing(
      trace_config, content::TracingController::StartTracingDoneCallback());
}

bool AwTracingController::StopAndFlush(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  return content::TracingController::GetInstance()->StopTracing(
      AwTraceDataEndpoint::Create(
          base::Bind(&AwTracingController::OnTraceDataReceived,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&AwTracingController::OnTraceDataComplete,
                     weak_factory_.GetWeakPtr())));
}

void AwTracingController::OnTraceDataComplete(
    std::unique_ptr<const base::DictionaryValue> metadata) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_object_.get(env);
  if (obj.obj()) {
    Java_AwTracingController_onTraceDataComplete(env, obj);
  }
}

void AwTracingController::OnTraceDataReceived(
    std::unique_ptr<std::string> chunk) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_object_.get(env);
  if (obj.obj()) {
    base::android::ScopedJavaLocalRef<jbyteArray> java_trace_data =
        StringToJavaBytes(env, *chunk);
    Java_AwTracingController_onTraceDataChunkReceived(env, obj,
                                                      java_trace_data);
  }
}

bool AwTracingController::IsTracing(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  return content::TracingController::GetInstance()->IsTracing();
}

}  // namespace android_webview
