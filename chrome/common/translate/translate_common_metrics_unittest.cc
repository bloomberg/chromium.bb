// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/translate/translate_common_metrics.h"

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

const int kTrue = 1;
const int kFalse = 0;

class MetricsRecorder {
 public:
  explicit MetricsRecorder(const char* key) : key_(key) {
    StatisticsRecorder::Initialize();

    HistogramBase* histogram = StatisticsRecorder::FindHistogram(key_);
    if (histogram)
      base_samples_ = histogram->SnapshotSamples();
  }

  void CheckLanguage(TranslateCommonMetrics::MetricsNameIndex index,
                     int expected_not_provided,
                     int expected_valid,
                     int expected_invalid) {
    ASSERT_EQ(TranslateCommonMetrics::GetMetricsName(index), key_);

    Snapshot();

    EXPECT_EQ(expected_not_provided,
              GetCountWithoutSnapshot(
                  TranslateCommonMetrics::LANGUAGE_NOT_PROVIDED));
    EXPECT_EQ(expected_valid,
              GetCountWithoutSnapshot(
                  TranslateCommonMetrics::LANGUAGE_VALID));
    EXPECT_EQ(expected_invalid,
              GetCountWithoutSnapshot(
                  TranslateCommonMetrics::LANGUAGE_INVALID));
  }

  void CheckLanguageVerification(int expected_cld_disabled,
                                 int expected_cld_only,
                                 int expected_unknown,
                                 int expected_cld_agree,
                                 int expected_cld_disagree,
                                 int expected_trust_cld,
                                 int expected_cld_complement_sub_code) {
    ASSERT_EQ(TranslateCommonMetrics::GetMetricsName(
        TranslateCommonMetrics::UMA_LANGUAGE_VERIFICATION), key_);

    Snapshot();

    EXPECT_EQ(
        expected_cld_disabled,
        GetCountWithoutSnapshot(
            TranslateCommonMetrics::LANGUAGE_VERIFICATION_CLD_DISABLED));
    EXPECT_EQ(
        expected_cld_only,
        GetCountWithoutSnapshot(
            TranslateCommonMetrics::LANGUAGE_VERIFICATION_CLD_ONLY));
    EXPECT_EQ(
        expected_unknown,
        GetCountWithoutSnapshot(
            TranslateCommonMetrics::LANGUAGE_VERIFICATION_UNKNOWN));
    EXPECT_EQ(
        expected_cld_agree,
        GetCountWithoutSnapshot(
            TranslateCommonMetrics::LANGUAGE_VERIFICATION_CLD_AGREE));
    EXPECT_EQ(
        expected_cld_disagree,
        GetCountWithoutSnapshot(
            TranslateCommonMetrics::LANGUAGE_VERIFICATION_CLD_DISAGREE));
    EXPECT_EQ(
        expected_trust_cld,
        GetCountWithoutSnapshot(
            TranslateCommonMetrics::LANGUAGE_VERIFICATION_TRUST_CLD));
    EXPECT_EQ(
        expected_cld_complement_sub_code,
        GetCountWithoutSnapshot(
            TranslateCommonMetrics::
            LANGUAGE_VERIFICATION_CLD_COMPLEMENT_SUB_CODE));
  }

  void CheckScheme(int expected_http, int expected_https, int expected_others) {
    ASSERT_EQ(TranslateCommonMetrics::GetMetricsName(
        TranslateCommonMetrics::UMA_PAGE_SCHEME), key_);

    Snapshot();

    EXPECT_EQ(expected_http,
              GetCountWithoutSnapshot(TranslateCommonMetrics::SCHEME_HTTP));
    EXPECT_EQ(expected_https,
              GetCountWithoutSnapshot(TranslateCommonMetrics::SCHEME_HTTPS));
    EXPECT_EQ(expected_others,
              GetCountWithoutSnapshot(TranslateCommonMetrics::SCHEME_OTHERS));
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

  HistogramBase::Count GetCount(HistogramBase::Sample value) {
    Snapshot();
    return GetCountWithoutSnapshot(value);
  }

 private:
  void Snapshot() {
    HistogramBase* histogram = StatisticsRecorder::FindHistogram(key_);
    if (!histogram)
      return;
    samples_ = histogram->SnapshotSamples();
  }

  HistogramBase::Count GetCountWithoutSnapshot(HistogramBase::Sample value) {
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

TEST(TranslateCommonMetricsTest, ReportContentLanguage) {
  MetricsRecorder recorder(TranslateCommonMetrics::GetMetricsName(
      TranslateCommonMetrics::UMA_CONTENT_LANGUAGE));

  recorder.CheckLanguage(TranslateCommonMetrics::UMA_CONTENT_LANGUAGE, 0, 0, 0);
  TranslateCommonMetrics::ReportContentLanguage(std::string(), std::string());
  recorder.CheckLanguage(TranslateCommonMetrics::UMA_CONTENT_LANGUAGE, 1, 0, 0);
  TranslateCommonMetrics::ReportContentLanguage("ja_JP", "ja-JP");
  recorder.CheckLanguage(TranslateCommonMetrics::UMA_CONTENT_LANGUAGE, 1, 0, 1);
  TranslateCommonMetrics::ReportContentLanguage("en", "en");
  recorder.CheckLanguage(TranslateCommonMetrics::UMA_CONTENT_LANGUAGE, 1, 1, 1);
}

TEST(TranslateCommonMetricsTest, ReportHtmlLang) {
  MetricsRecorder recorder(TranslateCommonMetrics::GetMetricsName(
      TranslateCommonMetrics::UMA_HTML_LANG));

  recorder.CheckLanguage(TranslateCommonMetrics::UMA_HTML_LANG, 0, 0, 0);
  TranslateCommonMetrics::ReportHtmlLang(std::string(), std::string());
  recorder.CheckLanguage(TranslateCommonMetrics::UMA_HTML_LANG, 1, 0, 0);
  TranslateCommonMetrics::ReportHtmlLang("ja_JP", "ja-JP");
  recorder.CheckLanguage(TranslateCommonMetrics::UMA_HTML_LANG, 1, 0, 1);
  TranslateCommonMetrics::ReportHtmlLang("en", "en");
  recorder.CheckLanguage(TranslateCommonMetrics::UMA_HTML_LANG, 1, 1, 1);
}

TEST(TranslateCommonMetricsTest, ReportLanguageVerification) {
  MetricsRecorder recorder(TranslateCommonMetrics::GetMetricsName(
      TranslateCommonMetrics::UMA_LANGUAGE_VERIFICATION));

  recorder.CheckLanguageVerification(0, 0, 0, 0, 0, 0, 0);
  TranslateCommonMetrics::ReportLanguageVerification(
      TranslateCommonMetrics::LANGUAGE_VERIFICATION_CLD_DISABLED);
  recorder.CheckLanguageVerification(1, 0, 0, 0, 0, 0, 0);
  TranslateCommonMetrics::ReportLanguageVerification(
      TranslateCommonMetrics::LANGUAGE_VERIFICATION_CLD_ONLY);
  recorder.CheckLanguageVerification(1, 1, 0, 0, 0, 0, 0);
  TranslateCommonMetrics::ReportLanguageVerification(
      TranslateCommonMetrics::LANGUAGE_VERIFICATION_UNKNOWN);
  recorder.CheckLanguageVerification(1, 1, 1, 0, 0, 0, 0);
  TranslateCommonMetrics::ReportLanguageVerification(
      TranslateCommonMetrics::LANGUAGE_VERIFICATION_CLD_AGREE);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 0, 0, 0);
  TranslateCommonMetrics::ReportLanguageVerification(
      TranslateCommonMetrics::LANGUAGE_VERIFICATION_CLD_DISAGREE);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 0, 0);
  TranslateCommonMetrics::ReportLanguageVerification(
      TranslateCommonMetrics::LANGUAGE_VERIFICATION_TRUST_CLD);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 1, 0);
  TranslateCommonMetrics::ReportLanguageVerification(
      TranslateCommonMetrics::LANGUAGE_VERIFICATION_CLD_COMPLEMENT_SUB_CODE);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 1, 1);
}

TEST(TranslateCommonMetricsTest, ReportTimeToBeReady) {
  MetricsRecorder recorder(TranslateCommonMetrics::GetMetricsName(
      TranslateCommonMetrics::UMA_TIME_TO_BE_READY));
  recorder.CheckTotalCount(0);
  TranslateCommonMetrics::ReportTimeToBeReady(3.14);
  recorder.CheckValueInLogs(3.14);
  recorder.CheckTotalCount(1);
}

TEST(TranslateCommonMetricsTest, ReportTimeToLoad) {
  MetricsRecorder recorder(TranslateCommonMetrics::GetMetricsName(
      TranslateCommonMetrics::UMA_TIME_TO_LOAD));
  recorder.CheckTotalCount(0);
  TranslateCommonMetrics::ReportTimeToLoad(573.0);
  recorder.CheckValueInLogs(573.0);
  recorder.CheckTotalCount(1);
}

TEST(TranslateCommonMetricsTest, ReportTimeToTranslate) {
  MetricsRecorder recorder(TranslateCommonMetrics::GetMetricsName(
      TranslateCommonMetrics::UMA_TIME_TO_TRANSLATE));
  recorder.CheckTotalCount(0);
  TranslateCommonMetrics::ReportTimeToTranslate(4649.0);
  recorder.CheckValueInLogs(4649.0);
  recorder.CheckTotalCount(1);
}

TEST(TranslateCommonMetricsTest, ReportUserActionDuration) {
  MetricsRecorder recorder(TranslateCommonMetrics::GetMetricsName(
      TranslateCommonMetrics::UMA_USER_ACTION_DURATION));
  recorder.CheckTotalCount(0);
  TimeTicks begin = TimeTicks::Now();
  TimeTicks end = begin + base::TimeDelta::FromSeconds(3776);
  TranslateCommonMetrics::ReportUserActionDuration(begin, end);
  recorder.CheckValueInLogs(3776000.0);
  recorder.CheckTotalCount(1);
}

TEST(TranslateCommonMetricsTest, ReportPageScheme) {
  MetricsRecorder recorder(TranslateCommonMetrics::GetMetricsName(
      TranslateCommonMetrics::UMA_PAGE_SCHEME));
  recorder.CheckScheme(0, 0, 0);
  TranslateCommonMetrics::ReportPageScheme("http");
  recorder.CheckScheme(1, 0, 0);
  TranslateCommonMetrics::ReportPageScheme("https");
  recorder.CheckScheme(1, 1, 0);
  TranslateCommonMetrics::ReportPageScheme("ftp");
  recorder.CheckScheme(1, 1, 1);
}

TEST(TranslateCommonMetricsTest, ReportSimilarLanguageMatch) {
  MetricsRecorder recorder(TranslateCommonMetrics::GetMetricsName(
      TranslateCommonMetrics::UMA_SIMILAR_LANGUAGE_MATCH));
  recorder.CheckTotalCount(0);
  EXPECT_EQ(0, recorder.GetCount(kTrue));
  EXPECT_EQ(0, recorder.GetCount(kFalse));
  TranslateCommonMetrics::ReportSimilarLanguageMatch(true);
  EXPECT_EQ(1, recorder.GetCount(kTrue));
  EXPECT_EQ(0, recorder.GetCount(kFalse));
  TranslateCommonMetrics::ReportSimilarLanguageMatch(false);
  EXPECT_EQ(1, recorder.GetCount(kTrue));
  EXPECT_EQ(1, recorder.GetCount(kFalse));
}

TEST(TranslateCommonMetricsTest, ReportLanguageDetectionTime) {
  MetricsRecorder recorder(TranslateCommonMetrics::GetMetricsName(
      TranslateCommonMetrics::UMA_LANGUAGE_DETECTION));
  recorder.CheckTotalCount(0);
  TimeTicks begin = TimeTicks::Now();
  TimeTicks end = begin + base::TimeDelta::FromMicroseconds(9009);
  TranslateCommonMetrics::ReportLanguageDetectionTime(begin, end);
  recorder.CheckValueInLogs(9.009);
  recorder.CheckTotalCount(1);
}
