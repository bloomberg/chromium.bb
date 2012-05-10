// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/trace_event_binding.h"

#include <jni.h>

#include <set>

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "jni/trace_event_jni.h"

namespace {

const char kJavaCategory[] = "Java";

// Boilerplate for safely converting Java data to TRACE_EVENT data.
class TraceEventDataConverter {
 public:
  TraceEventDataConverter(JNIEnv* env,
                          jstring jname,
                          jstring jarg)
      : env_(env),
      jname_(jname),
      jarg_(jarg),
      name_(env->GetStringUTFChars(jname, NULL)),
      arg_(jarg ? env->GetStringUTFChars(jarg, NULL) : NULL) {
  }
  ~TraceEventDataConverter() {
    env_->ReleaseStringUTFChars(jname_, name_);
    if (jarg_)
      env_->ReleaseStringUTFChars(jarg_, arg_);
  }

  // Return saves values to pass to TRACE_EVENT macros.
  const char* name() { return name_; }
  const char* arg_name() { return arg_ ? "arg" : NULL; }
  const char* arg() { return arg_; }

 private:
  JNIEnv* env_;
  jstring jname_;
  jstring jarg_;
  const char* name_;
  const char* arg_;

  DISALLOW_COPY_AND_ASSIGN(TraceEventDataConverter);
};

}  // namespace

static jboolean TraceEnabled(JNIEnv* env, jclass clazz) {
  return base::debug::TraceLog::GetInstance()->IsEnabled();
}

static void Instant(JNIEnv* env, jclass clazz,
                    jstring jname, jstring jarg) {
  TraceEventDataConverter converter(env, jname, jarg);
  if (converter.arg()) {
    TRACE_EVENT_COPY_INSTANT1(kJavaCategory, converter.name(),
                              converter.arg_name(), converter.arg());
  } else {
    TRACE_EVENT_COPY_INSTANT0(kJavaCategory, converter.name());
  }
}

static void Begin(JNIEnv* env, jclass clazz,
                  jstring jname, jstring jarg) {
  TraceEventDataConverter converter(env, jname, jarg);
  if (converter.arg()) {
    TRACE_EVENT_COPY_BEGIN1(kJavaCategory, converter.name(),
                       converter.arg_name(), converter.arg());
  } else {
    TRACE_EVENT_COPY_BEGIN0(kJavaCategory, converter.name());
  }
}

static void End(JNIEnv* env, jclass clazz,
                jstring jname, jstring jarg) {
  TraceEventDataConverter converter(env, jname, jarg);
  if (converter.arg()) {
    TRACE_EVENT_COPY_END1(kJavaCategory, converter.name(),
                     converter.arg_name(), converter.arg());
  } else {
    TRACE_EVENT_COPY_END0(kJavaCategory, converter.name());
  }
}

bool RegisterTraceEvent(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
