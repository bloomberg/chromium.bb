// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/android/contextualsearch/contextual_search_ranker_logger_impl.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/metrics/metrics_hashes.h"
#include "chrome/browser/browser_process.h"
#include "components/keyed_service/core/keyed_service.h"
#include "jni/ContextualSearchRankerLoggerImpl_jni.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

ContextualSearchRankerLoggerImpl::ContextualSearchRankerLoggerImpl(JNIEnv* env,
                                                                   jobject obj)
    : ukm_recorder_(nullptr), builder_(nullptr) {
  java_object_.Reset(env, obj);
}

ContextualSearchRankerLoggerImpl::~ContextualSearchRankerLoggerImpl() {
  java_object_ = nullptr;
}

void ContextualSearchRankerLoggerImpl::SetupLoggingAndRanker(
    JNIEnv* env,
    jobject obj,
    const base::android::JavaParamRef<jstring>& j_base_page_url) {
  GURL page_url =
      GURL(base::android::ConvertJavaStringToUTF8(env, j_base_page_url));
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  SetUkmRecorder(ukm_recorder, page_url);
  // TODO(donnd): set up the model once inference is available.
}

void ContextualSearchRankerLoggerImpl::SetUkmRecorder(
    ukm::UkmRecorder* ukm_recorder,
    const GURL& page_url) {
  if (!ukm_recorder) {
    builder_.reset();
    return;
  }

  ukm_recorder_ = ukm_recorder;
  source_id_ = ukm_recorder_->GetNewSourceID();
  ukm_recorder_->UpdateSourceURL(source_id_, page_url);
  builder_ = ukm_recorder_->GetEntryBuilder(source_id_, "ContextualSearch");
}

void ContextualSearchRankerLoggerImpl::LogLong(
    JNIEnv* env,
    jobject obj,
    const base::android::JavaParamRef<jstring>& j_feature,
    jlong j_long) {
  if (!builder_)
    return;

  std::string feature = base::android::ConvertJavaStringToUTF8(env, j_feature);
  builder_->AddMetric(feature.c_str(), j_long);
}

void ContextualSearchRankerLoggerImpl::WriteLogAndReset(JNIEnv* env,
                                                        jobject obj) {
  if (!ukm_recorder_)
    return;

  // Set up another builder for the next record (in case it's needed).
  builder_ = ukm_recorder_->GetEntryBuilder(source_id_, "ContextualSearch");
}

// Java wrapper boilerplate

void ContextualSearchRankerLoggerImpl::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  delete this;
}

jlong Init(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj) {
  ContextualSearchRankerLoggerImpl* ranker_logger_impl =
      new ContextualSearchRankerLoggerImpl(env, obj);
  return reinterpret_cast<intptr_t>(ranker_logger_impl);
}
