// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ukm_page_load_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/metrics_hashes.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/metrics/proto/ukm/entry.pb.h"
#include "components/ukm/test_ukm_service.h"
#include "components/ukm/ukm_entry.h"
#include "components/ukm/ukm_source.h"

namespace {

const char kTestUrl1[] = "https://www.google.com/";
const char kTestUrl2[] = "https://www.example.com/";

}  // namespace

class UkmPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::MakeUnique<UkmPageLoadMetricsObserver>());
  }

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();

    TestingBrowserProcess::GetGlobal()->SetUkmService(
        ukm_service_test_harness_.test_ukm_service());
  }

  size_t ukm_source_count() {
    return ukm_service_test_harness_.test_ukm_service()->sources_count();
  }

  size_t ukm_entry_count() {
    return ukm_service_test_harness_.test_ukm_service()->entries_count();
  }

  const ukm::UkmSource* GetUkmSource(size_t source_index) {
    return ukm_service_test_harness_.test_ukm_service()->GetSource(
        source_index);
  }

  const ukm::UkmEntry* GetUkmEntry(size_t entry_index) {
    return ukm_service_test_harness_.test_ukm_service()->GetEntry(entry_index);
  }

  static const ukm::Entry_Metric* FindMetric(
      const char* name,
      const google::protobuf::RepeatedPtrField<ukm::Entry_Metric>& metrics) {
    for (const auto& metric : metrics) {
      if (metric.metric_hash() == base::HashMetricName(name))
        return &metric;
    }
    return nullptr;
  }

  static bool HasMetric(
      const char* name,
      const google::protobuf::RepeatedPtrField<ukm::Entry_Metric>& metrics) {
    return FindMetric(name, metrics) != nullptr;
  }

  static void ExpectMetric(
      const char* name,
      int64_t expected_value,
      const google::protobuf::RepeatedPtrField<ukm::Entry_Metric>& metrics) {
    const ukm::Entry_Metric* metric = FindMetric(name, metrics);
    EXPECT_NE(nullptr, metric) << "Failed to find metric: " << name;
    EXPECT_EQ(expected_value, metric->value());
  }

 private:
  ukm::UkmServiceTestingHarness ukm_service_test_harness_;
};

TEST_F(UkmPageLoadMetricsObserverTest, NoMetrics) {
  EXPECT_EQ(0ul, ukm_source_count());
  EXPECT_EQ(0ul, ukm_entry_count());
}

TEST_F(UkmPageLoadMetricsObserverTest, FirstContentfulPaint) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_contentful_paint = base::TimeDelta::FromMilliseconds(300);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  EXPECT_EQ(1ul, ukm_source_count());
  const ukm::UkmSource* source = GetUkmSource(0);
  EXPECT_EQ(GURL(kTestUrl1), source->url());

  EXPECT_EQ(1ul, ukm_entry_count());
  const ukm::UkmEntry* entry = GetUkmEntry(0);
  EXPECT_EQ(entry->source_id(), source->id());

  ukm::Entry entry_proto;
  entry->PopulateProto(&entry_proto);
  EXPECT_EQ(entry_proto.source_id(), source->id());
  EXPECT_EQ(entry_proto.event_hash(),
            base::HashMetricName(internal::kUkmPageLoadEventName));
  EXPECT_GE(entry_proto.metrics_size(), 1);
  ExpectMetric(internal::kUkmFirstContentfulPaintName, 300,
               entry_proto.metrics());
}

TEST_F(UkmPageLoadMetricsObserverTest, MultiplePageLoads) {
  page_load_metrics::PageLoadTiming timing1;
  timing1.navigation_start = base::Time::FromDoubleT(1);
  timing1.first_contentful_paint = base::TimeDelta::FromMilliseconds(200);
  PopulateRequiredTimingFields(&timing1);

  // Second navigation reports no timing metrics.
  page_load_metrics::PageLoadTiming timing2;
  timing2.navigation_start = base::Time::FromDoubleT(1);
  timing2.first_contentful_paint = base::TimeDelta::FromMilliseconds(300);
  PopulateRequiredTimingFields(&timing2);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing1);

  NavigateAndCommit(GURL(kTestUrl2));
  SimulateTimingUpdate(timing2);

  // Simulate closing the tab.
  DeleteContents();

  EXPECT_EQ(2ul, ukm_source_count());
  const ukm::UkmSource* source1 = GetUkmSource(0);
  const ukm::UkmSource* source2 = GetUkmSource(1);
  EXPECT_EQ(GURL(kTestUrl1), source1->url());
  EXPECT_EQ(GURL(kTestUrl2), source2->url());
  EXPECT_NE(source1->id(), source2->id());

  EXPECT_EQ(2ul, ukm_entry_count());
  const ukm::UkmEntry* entry1 = GetUkmEntry(0);
  const ukm::UkmEntry* entry2 = GetUkmEntry(1);
  EXPECT_EQ(entry1->source_id(), source1->id());
  EXPECT_EQ(entry2->source_id(), source2->id());
  EXPECT_NE(entry1->source_id(), entry2->source_id());

  ukm::Entry entry1_proto;
  entry1->PopulateProto(&entry1_proto);
  ukm::Entry entry2_proto;
  entry2->PopulateProto(&entry2_proto);
  EXPECT_NE(entry1_proto.source_id(), entry2_proto.source_id());

  EXPECT_EQ(entry1_proto.source_id(), source1->id());
  EXPECT_EQ(entry1_proto.event_hash(),
            base::HashMetricName(internal::kUkmPageLoadEventName));
  EXPECT_GE(entry1_proto.metrics_size(), 1);
  ExpectMetric(internal::kUkmFirstContentfulPaintName, 200,
               entry1_proto.metrics());

  EXPECT_EQ(entry2_proto.source_id(), source2->id());
  EXPECT_EQ(entry2_proto.event_hash(),
            base::HashMetricName(internal::kUkmPageLoadEventName));
  EXPECT_GE(entry2_proto.metrics_size(), 1);
  ExpectMetric(internal::kUkmFirstContentfulPaintName, 300,
               entry2_proto.metrics());
}
