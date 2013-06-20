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

  void CheckLanguage(TranslateHelperMetrics::MetricsNameIndex index,
                     int expected_not_provided,
                     int expected_valid,
                     int expected_invalid) {
    ASSERT_EQ(TranslateHelperMetrics::GetMetricsName(index), key_);

    Snapshot();

    EXPECT_EQ(expected_not_provided,
              GetCountWithoutSnapshot(
                  TranslateHelperMetrics::LANGUAGE_NOT_PROVIDED));
    EXPECT_EQ(expected_valid,
              GetCountWithoutSnapshot(
                  TranslateHelperMetrics::LANGUAGE_VALID));
    EXPECT_EQ(expected_invalid,
              GetCountWithoutSnapshot(
                  TranslateHelperMetrics::LANGUAGE_INVALID));
  }

  void CheckLanguageVerification(int expected_cld_disabled,
                                 int expected_cld_only,
                                 int expected_unknown,
                                 int expected_cld_agree,
                                 int expected_cld_disagree,
                                 int expected_trust_cld,
                                 int expected_cld_complement_sub_code) {
    ASSERT_EQ(TranslateHelperMetrics::GetMetricsName(
        TranslateHelperMetrics::UMA_LANGUAGE_VERIFICATION), key_);

    Snapshot();

    EXPECT_EQ(
        expected_cld_disabled,
        GetCountWithoutSnapshot(
            TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_DISABLED));
    EXPECT_EQ(
        expected_cld_only,
        GetCountWithoutSnapshot(
            TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_ONLY));
    EXPECT_EQ(
        expected_unknown,
        GetCountWithoutSnapshot(
            TranslateHelperMetrics::LANGUAGE_VERIFICATION_UNKNOWN));
    EXPECT_EQ(
        expected_cld_agree,
        GetCountWithoutSnapshot(
            TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_AGREE));
    EXPECT_EQ(
        expected_cld_disagree,
        GetCountWithoutSnapshot(
            TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_DISAGREE));
    EXPECT_EQ(
        expected_trust_cld,
        GetCountWithoutSnapshot(
            TranslateHelperMetrics::LANGUAGE_VERIFICATION_TRUST_CLD));
    EXPECT_EQ(
        expected_cld_complement_sub_code,
        GetCountWithoutSnapshot(
            TranslateHelperMetrics::
            LANGUAGE_VERIFICATION_CLD_COMPLEMENT_SUB_CODE));
  }

  void CheckScheme(int expected_http, int expected_https, int expected_others) {
    ASSERT_EQ(TranslateHelperMetrics::GetMetricsName(
        TranslateHelperMetrics::UMA_PAGE_SCHEME), key_);

    Snapshot();

    EXPECT_EQ(expected_http,
              GetCountWithoutSnapshot(TranslateHelperMetrics::SCHEME_HTTP));
    EXPECT_EQ(expected_https,
              GetCountWithoutSnapshot(TranslateHelperMetrics::SCHEME_HTTPS));
    EXPECT_EQ(expected_others,
              GetCountWithoutSnapshot(TranslateHelperMetrics::SCHEME_OTHERS));
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

TEST(TranslateHelperMetricsTest, ReportContentLanguage) {
  MetricsRecorder recorder(TranslateHelperMetrics::GetMetricsName(
      TranslateHelperMetrics::UMA_CONTENT_LANGUAGE));

  recorder.CheckLanguage(TranslateHelperMetrics::UMA_CONTENT_LANGUAGE, 0, 0, 0);
  TranslateHelperMetrics::ReportContentLanguage(std::string(), std::string());
  recorder.CheckLanguage(TranslateHelperMetrics::UMA_CONTENT_LANGUAGE, 1, 0, 0);
  TranslateHelperMetrics::ReportContentLanguage("ja_JP", "ja-JP");
  recorder.CheckLanguage(TranslateHelperMetrics::UMA_CONTENT_LANGUAGE, 1, 0, 1);
  TranslateHelperMetrics::ReportContentLanguage("en", "en");
  recorder.CheckLanguage(TranslateHelperMetrics::UMA_CONTENT_LANGUAGE, 1, 1, 1);
}

TEST(TranslateHelperMetricsTest, ReportHtmlLang) {
  MetricsRecorder recorder(TranslateHelperMetrics::GetMetricsName(
      TranslateHelperMetrics::UMA_HTML_LANG));

  recorder.CheckLanguage(TranslateHelperMetrics::UMA_HTML_LANG, 0, 0, 0);
  TranslateHelperMetrics::ReportHtmlLang(std::string(), std::string());
  recorder.CheckLanguage(TranslateHelperMetrics::UMA_HTML_LANG, 1, 0, 0);
  TranslateHelperMetrics::ReportHtmlLang("ja_JP", "ja-JP");
  recorder.CheckLanguage(TranslateHelperMetrics::UMA_HTML_LANG, 1, 0, 1);
  TranslateHelperMetrics::ReportHtmlLang("en", "en");
  recorder.CheckLanguage(TranslateHelperMetrics::UMA_HTML_LANG, 1, 1, 1);
}

TEST(TranslateHelperMetricsTest, ReportLanguageVerification) {
  MetricsRecorder recorder(TranslateHelperMetrics::GetMetricsName(
      TranslateHelperMetrics::UMA_LANGUAGE_VERIFICATION));

  recorder.CheckLanguageVerification(0, 0, 0, 0, 0, 0, 0);
  TranslateHelperMetrics::ReportLanguageVerification(
      TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_DISABLED);
  recorder.CheckLanguageVerification(1, 0, 0, 0, 0, 0, 0);
  TranslateHelperMetrics::ReportLanguageVerification(
      TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_ONLY);
  recorder.CheckLanguageVerification(1, 1, 0, 0, 0, 0, 0);
  TranslateHelperMetrics::ReportLanguageVerification(
      TranslateHelperMetrics::LANGUAGE_VERIFICATION_UNKNOWN);
  recorder.CheckLanguageVerification(1, 1, 1, 0, 0, 0, 0);
  TranslateHelperMetrics::ReportLanguageVerification(
      TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_AGREE);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 0, 0, 0);
  TranslateHelperMetrics::ReportLanguageVerification(
      TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_DISAGREE);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 0, 0);
  TranslateHelperMetrics::ReportLanguageVerification(
      TranslateHelperMetrics::LANGUAGE_VERIFICATION_TRUST_CLD);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 1, 0);
  TranslateHelperMetrics::ReportLanguageVerification(
      TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_COMPLEMENT_SUB_CODE);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 1, 1);
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

TEST(TranslateHelperMetricsTest, ReportPageScheme) {
  MetricsRecorder recorder(TranslateHelperMetrics::GetMetricsName(
      TranslateHelperMetrics::UMA_PAGE_SCHEME));
  recorder.CheckScheme(0, 0, 0);
  TranslateHelperMetrics::ReportPageScheme("http");
  recorder.CheckScheme(1, 0, 0);
  TranslateHelperMetrics::ReportPageScheme("https");
  recorder.CheckScheme(1, 1, 0);
  TranslateHelperMetrics::ReportPageScheme("ftp");
  recorder.CheckScheme(1, 1, 1);
}

TEST(TranslateHelperMetricsTest, ReportSimilarLanguageMatch) {
  MetricsRecorder recorder(TranslateHelperMetrics::GetMetricsName(
      TranslateHelperMetrics::UMA_SIMILAR_LANGUAGE_MATCH));
  recorder.CheckTotalCount(0);
  EXPECT_EQ(0, recorder.GetCount(kTrue));
  EXPECT_EQ(0, recorder.GetCount(kFalse));
  TranslateHelperMetrics::ReportSimilarLanguageMatch(true);
  EXPECT_EQ(1, recorder.GetCount(kTrue));
  EXPECT_EQ(0, recorder.GetCount(kFalse));
  TranslateHelperMetrics::ReportSimilarLanguageMatch(false);
  EXPECT_EQ(1, recorder.GetCount(kTrue));
  EXPECT_EQ(1, recorder.GetCount(kFalse));
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
