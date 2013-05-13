// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_manager_metrics.h"

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

  void CheckInitiationStatus(int expected_disabled_by_prefs,
                             int expected_disabled_by_switch,
                             int expected_disabled_by_config,
                             int expected_language_is_not_supported,
                             int expected_url_is_not_supported,
                             int expected_similar_languages,
                             int expected_accept_languages,
                             int expected_auto_by_config,
                             int expected_auto_by_link,
                             int expected_show_infobar) {
    Snapshot();

    EXPECT_EQ(expected_disabled_by_prefs, GetCount(
        TranslateManagerMetrics::INITIATION_STATUS_DISABLED_BY_PREFS));
    EXPECT_EQ(expected_disabled_by_switch, GetCount(
        TranslateManagerMetrics::INITIATION_STATUS_DISABLED_BY_SWITCH));
    EXPECT_EQ(expected_disabled_by_config, GetCount(
        TranslateManagerMetrics::INITIATION_STATUS_DISABLED_BY_CONFIG));
    EXPECT_EQ(expected_language_is_not_supported, GetCount(
        TranslateManagerMetrics::INITIATION_STATUS_LANGUAGE_IS_NOT_SUPPORTED));
    EXPECT_EQ(expected_url_is_not_supported, GetCount(
        TranslateManagerMetrics::INITIATION_STATUS_URL_IS_NOT_SUPPORTED));
    EXPECT_EQ(expected_similar_languages, GetCount(
        TranslateManagerMetrics::INITIATION_STATUS_SIMILAR_LANGUAGES));
    EXPECT_EQ(expected_accept_languages, GetCount(
        TranslateManagerMetrics::INITIATION_STATUS_ACCEPT_LANGUAGES));
    EXPECT_EQ(expected_auto_by_config, GetCount(
        TranslateManagerMetrics::INITIATION_STATUS_AUTO_BY_CONFIG));
    EXPECT_EQ(expected_auto_by_link, GetCount(
        TranslateManagerMetrics::INITIATION_STATUS_AUTO_BY_LINK));
    EXPECT_EQ(expected_show_infobar, GetCount(
        TranslateManagerMetrics::INITIATION_STATUS_SHOW_INFOBAR));
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

TEST(TranslateManagerMetricsTest, ReportInitiationStatus) {
  MetricsRecorder recorder(TranslateManagerMetrics::GetMetricsName(
      TranslateManagerMetrics::UMA_INITIATION_STATUS));

  recorder.CheckInitiationStatus(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  TranslateManagerMetrics::ReportInitiationStatus(
      TranslateManagerMetrics::INITIATION_STATUS_DISABLED_BY_PREFS);
  recorder.CheckInitiationStatus(1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  TranslateManagerMetrics::ReportInitiationStatus(
      TranslateManagerMetrics::INITIATION_STATUS_DISABLED_BY_SWITCH);
  recorder.CheckInitiationStatus(1, 1, 0, 0, 0, 0, 0, 0, 0, 0);
  TranslateManagerMetrics::ReportInitiationStatus(
      TranslateManagerMetrics::INITIATION_STATUS_DISABLED_BY_CONFIG);
  recorder.CheckInitiationStatus(1, 1, 1, 0, 0, 0, 0, 0, 0, 0);
  TranslateManagerMetrics::ReportInitiationStatus(
      TranslateManagerMetrics::INITIATION_STATUS_LANGUAGE_IS_NOT_SUPPORTED);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 0, 0, 0, 0, 0, 0);
  TranslateManagerMetrics::ReportInitiationStatus(
      TranslateManagerMetrics::INITIATION_STATUS_URL_IS_NOT_SUPPORTED);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 0, 0, 0, 0, 0);
  TranslateManagerMetrics::ReportInitiationStatus(
      TranslateManagerMetrics::INITIATION_STATUS_SIMILAR_LANGUAGES);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 1, 0, 0, 0, 0);
  TranslateManagerMetrics::ReportInitiationStatus(
      TranslateManagerMetrics::INITIATION_STATUS_ACCEPT_LANGUAGES);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 1, 1, 0, 0, 0);
  TranslateManagerMetrics::ReportInitiationStatus(
      TranslateManagerMetrics::INITIATION_STATUS_AUTO_BY_CONFIG);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 1, 1, 1, 0, 0);
  TranslateManagerMetrics::ReportInitiationStatus(
      TranslateManagerMetrics::INITIATION_STATUS_AUTO_BY_LINK);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 1, 1, 1, 1, 0);
  TranslateManagerMetrics::ReportInitiationStatus(
      TranslateManagerMetrics::INITIATION_STATUS_SHOW_INFOBAR);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
}
