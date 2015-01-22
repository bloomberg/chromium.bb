// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/record_histogram.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "jni/RecordHistogram_jni.h"

namespace base {
namespace android {

void RecordBooleanHistogram(JNIEnv* env,
                            jclass clazz,
                            jstring j_histogram_name,
                            jboolean j_sample) {
  std::string histogram_name = ConvertJavaStringToUTF8(env, j_histogram_name);
  bool sample = static_cast<bool>(j_sample);

  BooleanHistogram::FactoryGet(histogram_name,
                               HistogramBase::kUmaTargetedHistogramFlag)
      ->AddBoolean(sample);
}

void RecordEnumeratedHistogram(JNIEnv* env,
                               jclass clazz,
                               jstring j_histogram_name,
                               jint j_sample,
                               jint j_boundary) {
  std::string histogram_name = ConvertJavaStringToUTF8(env, j_histogram_name);
  int sample = static_cast<int>(j_sample);
  int boundary = static_cast<int>(j_boundary);

  LinearHistogram::FactoryGet(histogram_name, 1, boundary, boundary + 1,
                              HistogramBase::kUmaTargetedHistogramFlag)
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
