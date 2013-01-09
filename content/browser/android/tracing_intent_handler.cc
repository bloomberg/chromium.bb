// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/tracing_intent_handler.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "content/public/browser/trace_controller.h"
#include "jni/TracingIntentHandler_jni.h"

namespace content {

TracingIntentHandler* g_trace_intent_handler = NULL;

TracingIntentHandler::TracingIntentHandler(const FilePath& path)
    : TraceSubscriberStdio(path) {
  TraceController::GetInstance()->BeginTracing(this, std::string("-test*"));
}

TracingIntentHandler::~TracingIntentHandler() {
}

void TracingIntentHandler::OnEndTracingComplete() {
  TraceSubscriberStdio::OnEndTracingComplete();
  delete this;
}

void TracingIntentHandler::OnEndTracing() {
  if (!TraceController::GetInstance()->EndTracingAsync(this)) {
    delete this;
  }
}

static void BeginTracing(JNIEnv* env, jclass clazz, jstring jspath) {
  std::string path(base::android::ConvertJavaStringToUTF8(env, jspath));
  if (g_trace_intent_handler != NULL)
    return;
  g_trace_intent_handler = new TracingIntentHandler(FilePath(path));
}

static void EndTracing(JNIEnv* env, jclass clazz) {
  DCHECK(!g_trace_intent_handler);
  g_trace_intent_handler->OnEndTracing();
  g_trace_intent_handler = NULL;
}

bool RegisterTracingIntentHandler(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
