// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/record_histogram.h"

#include <map>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/synchronization/lock.h"
#include "jni/RecordHistogram_jni.h"

namespace base {
namespace android {
namespace {

// Simple thread-safe wrapper for caching histograms. This avoids
// relatively expensive JNI string translation for each recording.
class HistogramCache {
 public:
  HistogramCache() {}

  HistogramBase* BooleanHistogram(JNIEnv* env,
                                  jstring j_histogram_name,
                                  jint j_histogram_key) {
    DCHECK(j_histogram_name);
    DCHECK(j_histogram_key);
    HistogramBase* histogram = FindLocked(j_histogram_key);
    if (histogram)
      return histogram;

    std::string histogram_name = ConvertJavaStringToUTF8(env, j_histogram_name);
    histogram = BooleanHistogram::FactoryGet(
        histogram_name, HistogramBase::kUmaTargetedHistogramFlag);
    return InsertLocked(j_histogram_key, histogram);
  }

  HistogramBase* EnumeratedHistogram(JNIEnv* env,
                                     jstring j_histogram_name,
                                     jint j_histogram_key,
                                     jint j_boundary) {
    DCHECK(j_histogram_name);
    DCHECK(j_histogram_key);
    HistogramBase* histogram = FindLocked(j_histogram_key);
    if (histogram)
      return histogram;

    std::string histogram_name = ConvertJavaStringToUTF8(env, j_histogram_name);
    // Note: This caching bypasses the boundary validation that occurs between
    // repeated lookups by the same name. It is up to the caller to ensure that
    // the provided boundary remains consistent.
    int boundary = static_cast<int>(j_boundary);
    histogram =
        LinearHistogram::FactoryGet(histogram_name, 1, boundary, boundary + 1,
                                    HistogramBase::kUmaTargetedHistogramFlag);
    return InsertLocked(j_histogram_key, histogram);
  }

 private:
  HistogramBase* FindLocked(jint j_histogram_key) {
    base::AutoLock locked(lock_);
    auto histogram_it = histograms_.find(j_histogram_key);
    return histogram_it != histograms_.end() ? histogram_it->second : nullptr;
  }

  HistogramBase* InsertLocked(jint j_histogram_key, HistogramBase* histogram) {
    base::AutoLock locked(lock_);
    histograms_.insert(std::make_pair(j_histogram_key, histogram));
    return histogram;
  }

  base::Lock lock_;
  std::map<jint, HistogramBase*> histograms_;

  DISALLOW_COPY_AND_ASSIGN(HistogramCache);
};

base::LazyInstance<HistogramCache>::Leaky g_histograms;

}  // namespace

void RecordBooleanHistogram(JNIEnv* env,
                            jclass clazz,
                            jstring j_histogram_name,
                            jint j_histogram_key,
                            jboolean j_sample) {
  bool sample = static_cast<bool>(j_sample);
  g_histograms.Get()
      .BooleanHistogram(env, j_histogram_name, j_histogram_key)
      ->AddBoolean(sample);
}

void RecordEnumeratedHistogram(JNIEnv* env,
                               jclass clazz,
                               jstring j_histogram_name,
                               jint j_histogram_key,
                               jint j_sample,
                               jint j_boundary) {
  int sample = static_cast<int>(j_sample);

  g_histograms.Get()
      .EnumeratedHistogram(env, j_histogram_name, j_histogram_key, j_boundary)
      ->Add(sample);
}

void Initialize(JNIEnv* env, jclass) {
  StatisticsRecorder::Initialize();
}

// This backs a Java test util for testing histograms -
// MetricsUtils.HistogramDelta. It should live in a test-specific file, but we
// currently can't have test-specific native code packaged in test-specific Java
// targets - see http://crbug.com/415945.
jint GetHistogramValueCountForTesting(JNIEnv* env,
                                      jclass clazz,
                                      jstring histogram_name,
                                      jint sample) {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(
      android::ConvertJavaStringToUTF8(env, histogram_name));
  if (histogram == nullptr) {
    // No samples have been recorded for this histogram (yet?).
    return 0;
  }

  scoped_ptr<HistogramSamples> samples = histogram->SnapshotSamples();
  return samples->GetCount(static_cast<int>(sample));
}

bool RegisterRecordHistogram(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace base
