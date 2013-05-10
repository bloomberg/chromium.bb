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
using base::StatisticsRecorder;

namespace {

const char kTranslateContentLanguage[] = "Translate.ContentLanguage";
const char kTranslateLanguageVerification[] = "Translate.LanguageVerification";

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
    ASSERT_EQ(kTranslateContentLanguage, key_);

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
    ASSERT_EQ(kTranslateLanguageVerification, key_);

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

  std::string key_;
  scoped_ptr<HistogramSamples> base_samples_;
  scoped_ptr<HistogramSamples> samples_;

  DISALLOW_COPY_AND_ASSIGN(MetricsRecorder);
};

}  // namespace

TEST(TranslateHelperMetricsTest, ReportContentLanguage) {
  MetricsRecorder recorder(kTranslateContentLanguage);

  recorder.CheckContentLanguage(0, 0, 0);
  TranslateHelperMetrics::ReportContentLanguage(std::string(), std::string());
  recorder.CheckContentLanguage(1, 0, 0);
  TranslateHelperMetrics::ReportContentLanguage("ja_JP", "ja-JP");
  recorder.CheckContentLanguage(1, 0, 1);
  TranslateHelperMetrics::ReportContentLanguage("en", "en");
  recorder.CheckContentLanguage(1, 1, 1);
}

TEST(TranslateHelperMetricsTest, ReportLanguageVerification) {
  MetricsRecorder recorder(kTranslateLanguageVerification);

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
