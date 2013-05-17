// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/translate/translate_helper_metrics.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::HistogramBase;
using base::HistogramSamples;
using base::SampleCountIterator;
using base::StatisticsRecorder;
using base::TimeTicks;

namespace {

class MetricsRecorder {
 public:
  explicit MetricsRecorder(const char* key)
      : key_(key),
        base_samples_(NULL),
        samples_(NULL) {
    StatisticsRecorder::Initialize();

    HistogramBase* histogram = StatisticsRecorder::FindHistogram(key_);
    if (histogram)
      base_samples_ = histogram->SnapshotSamples();
  }

  void CheckContentLanguage(int expected_not_provided,
                            int expected_valid,
                            int expected_invalid) {
    ASSERT_EQ(TranslateHelperMetrics::GetMetricsName(
        TranslateHelperMetrics::UMA_CONTENT_LANGUAGE), key_);

    Snapshot();

    EXPECT_EQ(
        expected_not_provided,
        GetCount(TranslateHelperMetrics::CONTENT_LANGUAGE_NOT_PROVIDED));
    EXPECT_EQ(
        expected_valid,
        GetCount(TranslateHelperMetrics::CONTENT_LANGUAGE_VALID));
    EXPECT_EQ(
        expected_invalid,
        GetCount(TranslateHelperMetrics::CONTENT_LANGUAGE_INVALID));
  }

  void CheckLanguageVerification(int expected_cld_disabled,
                                 int expected_cld_only,
                                 int expected_unknown,
                                 int expected_cld_agree,
                                 int expected_cld_disagree) {
    ASSERT_EQ(TranslateHelperMetrics::GetMetricsName(
        TranslateHelperMetrics::UMA_LANGUAGE_VERIFICATION), key_);

    Snapshot();

    EXPECT_EQ(
        expected_cld_disabled,
        GetCount(TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_DISABLED));
    EXPECT_EQ(
        expected_cld_only,
        GetCount(TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_ONLY));
    EXPECT_EQ(
        expected_unknown,
        GetCount(TranslateHelperMetrics::LANGUAGE_VERIFICATION_UNKNOWN));
    EXPECT_EQ(
        expected_cld_agree,
        GetCount(TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_AGREE));
    EXPECT_EQ(
        expected_cld_disagree,
        GetCount(TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_DISAGREE));
  }

  void CheckTotalCount(int count) {
    Snapshot();
    EXPECT_EQ(count, GetTotalCount());
  }

  void CheckValueInLogs(double value) {
    Snapshot();
    ASSERT_TRUE(samples_.get());
    for (scoped_ptr<SampleCountIterator> i = samples_->Iterator();
         !i->Done();
         i->Next()) {
      HistogramBase::Sample min;
      HistogramBase::Sample max;
      HistogramBase::Count count;
      i->Get(&min, &max, &count);
      if (min <= value && value <= max && count >= 1)
        return;
    }
    EXPECT_FALSE(true);
  }

 private:
  void Snapshot() {
    HistogramBase* histogram = StatisticsRecorder::FindHistogram(key_);
    if (!histogram)
      return;
    samples_ = histogram->SnapshotSamples();
  }

  HistogramBase::Count GetCount(HistogramBase::Sample value) {
    if (!samples_.get())
      return 0;
    HistogramBase::Count count = samples_->GetCount(value);
    if (!base_samples_.get())
      return count;
    return count - base_samples_->GetCount(value);
  }

  HistogramBase::Count GetTotalCount() {
    if (!samples_.get())
      return 0;
    HistogramBase::Count count = samples_->TotalCount();
    if (!base_samples_.get())
      return count;
    return count - base_samples_->TotalCount();
  }

  std::string key_;
  scoped_ptr<HistogramSamples> base_samples_;
  scoped_ptr<HistogramSamples> samples_;

  DISALLOW_COPY_AND_ASSIGN(MetricsRecorder);
};

}  // namespace

TEST(TranslateHelperMetricsTest, ReportContentLanguage) {
  MetricsRecorder recorder(TranslateHelperMetrics::GetMetricsName(
      TranslateHelperMetrics::UMA_CONTENT_LANGUAGE));

  recorder.CheckContentLanguage(0, 0, 0);
  TranslateHelperMetrics::ReportContentLanguage(std::string(), std::string());
  recorder.CheckContentLanguage(1, 0, 0);
  TranslateHelperMetrics::ReportContentLanguage("ja_JP", "ja-JP");
  recorder.CheckContentLanguage(1, 0, 1);
  TranslateHelperMetrics::ReportContentLanguage("en", "en");
  recorder.CheckContentLanguage(1, 1, 1);
}

TEST(TranslateHelperMetricsTest, ReportLanguageVerification) {
  MetricsRecorder recorder(TranslateHelperMetrics::GetMetricsName(
      TranslateHelperMetrics::UMA_LANGUAGE_VERIFICATION));

  recorder.CheckLanguageVerification(0, 0, 0, 0, 0);
  TranslateHelperMetrics::ReportLanguageVerification(
      TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_DISABLED);
  recorder.CheckLanguageVerification(1, 0, 0, 0, 0);
  TranslateHelperMetrics::ReportLanguageVerification(
      TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_ONLY);
  recorder.CheckLanguageVerification(1, 1, 0, 0, 0);
  TranslateHelperMetrics::ReportLanguageVerification(
      TranslateHelperMetrics::LANGUAGE_VERIFICATION_UNKNOWN);
  recorder.CheckLanguageVerification(1, 1, 1, 0, 0);
  TranslateHelperMetrics::ReportLanguageVerification(
      TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_AGREE);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 0);
  TranslateHelperMetrics::ReportLanguageVerification(
      TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_DISAGREE);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1);
}

TEST(TranslateHelperMetricsTest, ReportTimeToBeReady) {
  MetricsRecorder recorder(TranslateHelperMetrics::GetMetricsName(
      TranslateHelperMetrics::UMA_TIME_TO_BE_READY));
  recorder.CheckTotalCount(0);
  TranslateHelperMetrics::ReportTimeToBeReady(3.14);
  recorder.CheckValueInLogs(3.14);
  recorder.CheckTotalCount(1);
}

TEST(TranslateHelperMetricsTest, ReportTimeToLoad) {
  MetricsRecorder recorder(TranslateHelperMetrics::GetMetricsName(
      TranslateHelperMetrics::UMA_TIME_TO_LOAD));
  recorder.CheckTotalCount(0);
  TranslateHelperMetrics::ReportTimeToLoad(573.0);
  recorder.CheckValueInLogs(573.0);
  recorder.CheckTotalCount(1);
}

TEST(TranslateHelperMetricsTest, ReportTimeToTranslate) {
  MetricsRecorder recorder(TranslateHelperMetrics::GetMetricsName(
      TranslateHelperMetrics::UMA_TIME_TO_TRANSLATE));
  recorder.CheckTotalCount(0);
  TranslateHelperMetrics::ReportTimeToTranslate(4649.0);
  recorder.CheckValueInLogs(4649.0);
  recorder.CheckTotalCount(1);
}

TEST(TranslateHelperMetricsTest, ReportUserActionDuration) {
  MetricsRecorder recorder(TranslateHelperMetrics::GetMetricsName(
      TranslateHelperMetrics::UMA_USER_ACTION_DURATION));
  recorder.CheckTotalCount(0);
  TimeTicks begin = TimeTicks::Now();
  TimeTicks end = begin + base::TimeDelta::FromSeconds(3776);
  TranslateHelperMetrics::ReportUserActionDuration(begin, end);
  recorder.CheckValueInLogs(3776000.0);
  recorder.CheckTotalCount(1);
}

#if defined(ENABLE_LANGUAGE_DETECTION)

TEST(TranslateHelperMetricsTest, ReportLanguageDetectionTime) {
  MetricsRecorder recorder(TranslateHelperMetrics::GetMetricsName(
      TranslateHelperMetrics::UMA_LANGUAGE_DETECTION));
  recorder.CheckTotalCount(0);
  TimeTicks begin = TimeTicks::Now();
  TimeTicks end = begin + base::TimeDelta::FromMicroseconds(9009);
  TranslateHelperMetrics::ReportLanguageDetectionTime(begin, end);
  recorder.CheckValueInLogs(9.009);
  recorder.CheckTotalCount(1);
}

#endif  // defined(ENABLE_LANGUAGE_DETECTION)
