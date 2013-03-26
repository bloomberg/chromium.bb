// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/trace_event_binding.h"

#include <jni.h>

#include <set>

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "jni/TraceEvent_jni.h"

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
  virtual ~TraceEventDataConverter() {
    env_->ReleaseStringUTFChars(jname_, name_);
    if (jarg_)
      env_->ReleaseStringUTFChars(jarg_, arg_);
  }

  // Return saves values to pass to TRACE_EVENT macros.
  const char* name() { return name_; }
  const char* arg_name() { return arg_ ? "arg" : NULL; }
  const char* arg() { return arg_; }

 protected:
  JNIEnv* env_;

 private:
  jstring jname_;
  jstring jarg_;
  const char* name_;
  const char* arg_;

  DISALLOW_COPY_AND_ASSIGN(TraceEventDataConverter);
};

class AsyncTraceEventDataConverter : public TraceEventDataConverter {
 public:
  AsyncTraceEventDataConverter(JNIEnv* env,
                               jstring jname,
                               jlong jid,
                               jstring jarg)
    : TraceEventDataConverter(env, jname, jarg),
      id_(jid) {
  }

  virtual ~AsyncTraceEventDataConverter() {
  }

  const long long id() { return id_; }

 private:
  long long id_;
};

}  // namespace

static jboolean TraceEnabled(JNIEnv* env, jclass clazz) {
  return base::debug::TraceLog::GetInstance()->IsEnabled();
}

static void StartATrace(JNIEnv* env, jclass clazz) {
  base::debug::TraceLog::GetInstance()->StartATrace();
}

static void StopATrace(JNIEnv* env, jclass clazz) {
  base::debug::TraceLog::GetInstance()->StopATrace();
}

static void Instant(JNIEnv* env, jclass clazz,
                    jstring jname, jstring jarg) {
  TraceEventDataConverter converter(env, jname, jarg);
  if (converter.arg()) {
    TRACE_EVENT_COPY_INSTANT1(kJavaCategory, converter.name(),
                              TRACE_EVENT_SCOPE_THREAD,
                              converter.arg_name(), converter.arg());
  } else {
    TRACE_EVENT_COPY_INSTANT0(kJavaCategory, converter.name(),
                              TRACE_EVENT_SCOPE_THREAD);
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

static void StartAsync(JNIEnv* env, jclass clazz,
                       jstring jname, jlong jid, jstring jarg) {
  AsyncTraceEventDataConverter converter(env, jname, jid, jarg);
  if (converter.arg()) {
    TRACE_EVENT_COPY_ASYNC_BEGIN1(kJavaCategory,
                                  converter.name(),
                                  converter.id(),
                                  converter.arg_name(),
                                  converter.arg());
  } else {
    TRACE_EVENT_COPY_ASYNC_BEGIN0(kJavaCategory,
                                  converter.name(),
                                  converter.id());
  }
}

static void FinishAsync(JNIEnv* env, jclass clazz,
                        jstring jname, jlong jid, jstring jarg) {
  AsyncTraceEventDataConverter converter(env, jname, jid, jarg);
  if (converter.arg()) {
    TRACE_EVENT_COPY_ASYNC_END1(kJavaCategory,
                                converter.name(),
                                converter.id(),
                                converter.arg_name(),
                                converter.arg());
  } else {
    TRACE_EVENT_COPY_ASYNC_END0(kJavaCategory,
                                converter.name(),
                                converter.id());
  }
}

bool RegisterTraceEvent(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
