// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/search/most_visited_iframe_source.h"
#include "testing/gtest/include/gtest/gtest.h"

class MostVisitedIframeSourceTest : public testing::Test {
 public:
  void ExpectNullData(base::RefCountedMemory* data) {
    EXPECT_EQ(NULL, data);
  }

 protected:
  MostVisitedIframeSource* source() { return source_.get(); }

 private:
  virtual void SetUp() { source_.reset(new MostVisitedIframeSource()); }

  scoped_ptr<MostVisitedIframeSource> source_;
};

TEST_F(MostVisitedIframeSourceTest, LogEndpointIsValidNoProvider) {
  content::URLDataSource::GotDataCallback callback = base::Bind(
      &MostVisitedIframeSourceTest::ExpectNullData, base::Unretained(this));

  base::StatisticsRecorder::Initialize();
  // Making sure the histogram is created.
  UMA_HISTOGRAM_ENUMERATION(MostVisitedIframeSource::kMostVisitedHistogramName,
                            0, MostVisitedIframeSource::kNumMostVisited);

  base::HistogramBase* histogram = base::StatisticsRecorder::FindHistogram(
      MostVisitedIframeSource::kMostVisitedHistogramName);

  scoped_ptr<base::HistogramSamples> samples1(histogram->SnapshotSamples());
  // The dummy value got logged.
  EXPECT_EQ(1, samples1->TotalCount());
  EXPECT_EQ(1, samples1->GetCount(0));

  // Test the method.
  source()->StartDataRequest("log.html?pos=1", 0, 0, callback);

  scoped_ptr<base::HistogramSamples> samples2(histogram->SnapshotSamples());
  EXPECT_EQ(samples1->TotalCount() + 1, samples2->TotalCount());
  EXPECT_EQ(1, samples2->GetCount(1));

  // Counts accumulate and behave as expected.
  source()->StartDataRequest("log.html?pos=5", 0, 0, callback);
  source()->StartDataRequest("log.html?pos=1", 0, 0, callback);

  scoped_ptr<base::HistogramSamples> samples3(histogram->SnapshotSamples());
  EXPECT_EQ(samples2->TotalCount() + 2, samples3->TotalCount());
  EXPECT_EQ(1, samples3->GetCount(5));
  EXPECT_EQ(2, samples3->GetCount(1));

  // If 'pos' is unspecified or invalid, callback still gets called but values
  // are not incremented.
  source()->StartDataRequest("log.html?pos=", 0, 0, callback);
  source()->StartDataRequest("log.html", 0, 0, callback);
  // Total count hasn't changed.
  EXPECT_EQ(samples2->TotalCount() + 2, samples3->TotalCount());
}

TEST_F(MostVisitedIframeSourceTest, LogEndpointIsValidWithProvider) {
  content::URLDataSource::GotDataCallback callback = base::Bind(
      &MostVisitedIframeSourceTest::ExpectNullData, base::Unretained(this));

  base::StatisticsRecorder::Initialize();
  // Making sure a test histogram with provider is created by logging a dummy
  // value.
  const std::string histogram_name =
      MostVisitedIframeSource::GetHistogramNameForProvider("foobar");
  UMA_HISTOGRAM_ENUMERATION(histogram_name, 0,
                            MostVisitedIframeSource::kNumMostVisited);

  base::HistogramBase* histogram = base::StatisticsRecorder::FindHistogram(
      histogram_name);
  scoped_ptr<base::HistogramSamples> samples1(histogram->SnapshotSamples());
  // The dummy value got logged.
  EXPECT_EQ(1, samples1->TotalCount());
  EXPECT_EQ(1, samples1->GetCount(0));

  // Test the method.
  source()->StartDataRequest("log.html?pos=1&pr=foobar", 0, 0, callback);

  scoped_ptr<base::HistogramSamples> samples2(histogram->SnapshotSamples());
  EXPECT_EQ(samples1->TotalCount() + 1, samples2->TotalCount());
  EXPECT_EQ(1, samples2->GetCount(1));

  // Counts accumulate and behave as expected.
  source()->StartDataRequest("log.html?pos=5&pr=foobar", 0, 0, callback);
  source()->StartDataRequest("log.html?pos=1&pr=foobar", 0, 0, callback);
  // This should have no effect for the "foobar" histogram.
  source()->StartDataRequest("log.html?pos=1&pr=nofoo", 0, 0, callback);

  scoped_ptr<base::HistogramSamples> samples3(histogram->SnapshotSamples());
  EXPECT_EQ(samples2->TotalCount() + 2, samples3->TotalCount());
  EXPECT_EQ(1, samples3->GetCount(5));
  EXPECT_EQ(2, samples3->GetCount(1));
}
