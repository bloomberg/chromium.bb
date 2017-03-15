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
#include "testing/gmock/include/gmock/gmock.h"

using testing::AnyNumber;
using testing::Mock;
using testing::Return;

namespace {

const char kTestUrl1[] = "https://www.google.com/";
const char kTestUrl2[] = "https://www.example.com/";

class MockNetworkQualityProvider
    : public net::NetworkQualityEstimator::NetworkQualityProvider {
 public:
  MOCK_CONST_METHOD0(GetEffectiveConnectionType,
                     net::EffectiveConnectionType());
  MOCK_METHOD1(
      AddEffectiveConnectionTypeObserver,
      void(net::NetworkQualityEstimator::EffectiveConnectionTypeObserver*));
  MOCK_METHOD1(
      RemoveEffectiveConnectionTypeObserver,
      void(net::NetworkQualityEstimator::EffectiveConnectionTypeObserver*));
  MOCK_METHOD1(
      AddRTTAndThroughputEstimatesObserver,
      void(net::NetworkQualityEstimator::RTTAndThroughputEstimatesObserver*));
  MOCK_METHOD1(
      RemoveRTTAndThroughputEstimatesObserver,
      void(net::NetworkQualityEstimator::RTTAndThroughputEstimatesObserver*));
};

}  // namespace

class UkmPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::MakeUnique<UkmPageLoadMetricsObserver>(
        &mock_network_quality_provider_));
  }

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();

    EXPECT_CALL(mock_network_quality_provider_, GetEffectiveConnectionType())
        .Times(AnyNumber())
        .WillRepeatedly(Return(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN));

    TestingBrowserProcess::GetGlobal()->SetUkmService(
        ukm_service_test_harness_.test_ukm_service());
  }

  size_t ukm_source_count() {
    return ukm_service_test_harness_.test_ukm_service()->sources_count();
  }

  size_t ukm_entry_count() {
    return ukm_service_test_harness_.test_ukm_service()->entries_count();
  }

  MockNetworkQualityProvider& mock_network_quality_provider() {
    return mock_network_quality_provider_;
  }

  const ukm::UkmSource* GetUkmSourceForUrl(const char* url) {
    return ukm_service_test_harness_.test_ukm_service()->GetSourceForUrl(url);
  }

  const ukm::UkmEntry* GetUkmEntry(size_t entry_index) {
    return ukm_service_test_harness_.test_ukm_service()->GetEntry(entry_index);
  }

  std::vector<const ukm::UkmEntry*> GetUkmEntriesForSourceID(
      int32_t source_id) {
    std::vector<const ukm::UkmEntry*> entries;
    for (size_t i = 0; i < ukm_entry_count(); ++i) {
      const ukm::UkmEntry* entry = GetUkmEntry(i);
      if (entry->source_id() == source_id)
        entries.push_back(entry);
    }
    return entries;
  }

  // Provides a single merged ukm::Entry proto that contains all metrics from
  // the given |entries|. |entries| must be non-empty, and all |entries| must
  // have the same |source_id| and |event_hash|.
  ukm::Entry GetMergedEntryProto(
      const std::vector<const ukm::UkmEntry*>& entries) {
    EXPECT_FALSE(entries.empty());
    ukm::Entry merged_entry;
    for (auto* entry : entries) {
      ukm::Entry entry_proto;
      entry->PopulateProto(&entry_proto);
      EXPECT_TRUE(entry_proto.has_source_id());
      EXPECT_TRUE(entry_proto.has_event_hash());
      if (merged_entry.has_source_id()) {
        EXPECT_EQ(merged_entry.source_id(), entry_proto.source_id());
        EXPECT_EQ(merged_entry.event_hash(), entry_proto.event_hash());
      }
      merged_entry.MergeFrom(entry_proto);
    }
    return merged_entry;
  }

  ukm::Entry GetMergedEntryProtoForSourceID(int32_t source_id) {
    ukm::Entry entry = GetMergedEntryProto(GetUkmEntriesForSourceID(source_id));
    EXPECT_EQ(source_id, entry.source_id());
    EXPECT_TRUE(entry.has_event_hash());
    return entry;
  }

  static const ukm::Entry_Metric* FindMetric(
      const char* name,
      const google::protobuf::RepeatedPtrField<ukm::Entry_Metric>& metrics)
      WARN_UNUSED_RESULT {
    for (const auto& metric : metrics) {
      if (metric.metric_hash() == base::HashMetricName(name))
        return &metric;
    }
    return nullptr;
  }

  static bool HasMetric(
      const char* name,
      const google::protobuf::RepeatedPtrField<ukm::Entry_Metric>& metrics)
      WARN_UNUSED_RESULT {
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
  MockNetworkQualityProvider mock_network_quality_provider_;
  ukm::UkmServiceTestingHarness ukm_service_test_harness_;
};

TEST_F(UkmPageLoadMetricsObserverTest, NoMetrics) {
  EXPECT_EQ(0ul, ukm_source_count());
  EXPECT_EQ(0ul, ukm_entry_count());
}

TEST_F(UkmPageLoadMetricsObserverTest, Basic) {
  // PageLoadTiming with all recorded metrics other than FMP. This allows us to
  // verify both that all metrics are logged, and that we don't log metrics that
  // aren't present in the PageLoadTiming struct. Logging of FMP is verified in
  // the FirstMeaningfulPaint test below.
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_start = base::TimeDelta::FromMilliseconds(100);
  timing.dom_content_loaded_event_start =
      base::TimeDelta::FromMilliseconds(200);
  timing.first_contentful_paint = base::TimeDelta::FromMilliseconds(300);
  timing.load_event_start = base::TimeDelta::FromMilliseconds(500);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  EXPECT_EQ(1ul, ukm_source_count());
  const ukm::UkmSource* source = GetUkmSourceForUrl(kTestUrl1);
  EXPECT_EQ(GURL(kTestUrl1), source->url());

  EXPECT_GE(ukm_entry_count(), 1ul);
  ukm::Entry entry_proto = GetMergedEntryProtoForSourceID(source->id());
  EXPECT_EQ(entry_proto.source_id(), source->id());
  EXPECT_EQ(entry_proto.event_hash(),
            base::HashMetricName(internal::kUkmPageLoadEventName));
  EXPECT_FALSE(entry_proto.metrics().empty());
  ExpectMetric(internal::kUkmPageTransition, ui::PAGE_TRANSITION_LINK,
               entry_proto.metrics());
  ExpectMetric(internal::kUkmParseStartName, 100, entry_proto.metrics());
  ExpectMetric(internal::kUkmDomContentLoadedName, 200, entry_proto.metrics());
  ExpectMetric(internal::kUkmFirstContentfulPaintName, 300,
               entry_proto.metrics());
  ExpectMetric(internal::kUkmLoadEventName, 500, entry_proto.metrics());
  EXPECT_FALSE(
      HasMetric(internal::kUkmFirstMeaningfulPaintName, entry_proto.metrics()));
  EXPECT_TRUE(
      HasMetric(internal::kUkmForegroundDurationName, entry_proto.metrics()));
}

TEST_F(UkmPageLoadMetricsObserverTest, FailedProvisionalLoad) {
  EXPECT_CALL(mock_network_quality_provider(), GetEffectiveConnectionType())
      .WillRepeatedly(Return(net::EFFECTIVE_CONNECTION_TYPE_2G));

  GURL url(kTestUrl1);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  rfh_tester->SimulateNavigationStart(url);
  rfh_tester->SimulateNavigationError(url, net::ERR_TIMED_OUT);
  rfh_tester->SimulateNavigationStop();

  // Simulate closing the tab.
  DeleteContents();

  EXPECT_EQ(1ul, ukm_source_count());
  const ukm::UkmSource* source = GetUkmSourceForUrl(kTestUrl1);
  EXPECT_EQ(GURL(kTestUrl1), source->url());

  EXPECT_GE(ukm_entry_count(), 1ul);
  ukm::Entry entry_proto = GetMergedEntryProtoForSourceID(source->id());
  EXPECT_EQ(entry_proto.source_id(), source->id());
  EXPECT_EQ(entry_proto.event_hash(),
            base::HashMetricName(internal::kUkmPageLoadEventName));

  // Make sure that only the following metrics are logged. In particular, no
  // paint/document/etc timing metrics should be logged for failed provisional
  // loads.
  EXPECT_EQ(5, entry_proto.metrics().size());
  ExpectMetric(internal::kUkmPageTransition, ui::PAGE_TRANSITION_LINK,
               entry_proto.metrics());
  ExpectMetric(internal::kUkmEffectiveConnectionType,
               net::EFFECTIVE_CONNECTION_TYPE_2G, entry_proto.metrics());
  ExpectMetric(internal::kUkmNetErrorCode,
               static_cast<int64_t>(net::ERR_TIMED_OUT) * -1,
               entry_proto.metrics());
  EXPECT_TRUE(
      HasMetric(internal::kUkmForegroundDurationName, entry_proto.metrics()));
  EXPECT_TRUE(
      HasMetric(internal::kUkmFailedProvisionaLoadName, entry_proto.metrics()));
}

TEST_F(UkmPageLoadMetricsObserverTest, FirstMeaningfulPaint) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_meaningful_paint = base::TimeDelta::FromMilliseconds(600);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  EXPECT_EQ(1ul, ukm_source_count());
  const ukm::UkmSource* source = GetUkmSourceForUrl(kTestUrl1);
  EXPECT_EQ(GURL(kTestUrl1), source->url());

  EXPECT_GE(ukm_entry_count(), 1ul);
  ukm::Entry entry_proto = GetMergedEntryProtoForSourceID(source->id());
  EXPECT_EQ(entry_proto.source_id(), source->id());
  EXPECT_EQ(entry_proto.event_hash(),
            base::HashMetricName(internal::kUkmPageLoadEventName));
  EXPECT_FALSE(entry_proto.metrics().empty());
  ExpectMetric(internal::kUkmFirstMeaningfulPaintName, 600,
               entry_proto.metrics());
  EXPECT_TRUE(
      HasMetric(internal::kUkmForegroundDurationName, entry_proto.metrics()));
}

TEST_F(UkmPageLoadMetricsObserverTest, MultiplePageLoads) {
  page_load_metrics::PageLoadTiming timing1;
  timing1.navigation_start = base::Time::FromDoubleT(1);
  timing1.first_contentful_paint = base::TimeDelta::FromMilliseconds(200);
  PopulateRequiredTimingFields(&timing1);

  // Second navigation reports no timing metrics.
  page_load_metrics::PageLoadTiming timing2;
  timing2.navigation_start = base::Time::FromDoubleT(1);
  PopulateRequiredTimingFields(&timing2);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing1);

  NavigateAndCommit(GURL(kTestUrl2));
  SimulateTimingUpdate(timing2);

  // Simulate closing the tab.
  DeleteContents();

  EXPECT_EQ(2ul, ukm_source_count());
  const ukm::UkmSource* source1 = GetUkmSourceForUrl(kTestUrl1);
  const ukm::UkmSource* source2 = GetUkmSourceForUrl(kTestUrl2);
  EXPECT_EQ(GURL(kTestUrl1), source1->url());
  EXPECT_EQ(GURL(kTestUrl2), source2->url());
  EXPECT_NE(source1->id(), source2->id());

  EXPECT_GE(ukm_entry_count(), 2ul);
  ukm::Entry entry1_proto = GetMergedEntryProtoForSourceID(source1->id());
  ukm::Entry entry2_proto = GetMergedEntryProtoForSourceID(source2->id());
  EXPECT_NE(entry1_proto.source_id(), entry2_proto.source_id());

  EXPECT_EQ(entry1_proto.source_id(), source1->id());
  EXPECT_EQ(entry1_proto.event_hash(),
            base::HashMetricName(internal::kUkmPageLoadEventName));
  EXPECT_FALSE(entry1_proto.metrics().empty());
  ExpectMetric(internal::kUkmFirstContentfulPaintName, 200,
               entry1_proto.metrics());
  EXPECT_FALSE(HasMetric(internal::kUkmFirstMeaningfulPaintName,
                         entry2_proto.metrics()));
  EXPECT_TRUE(
      HasMetric(internal::kUkmForegroundDurationName, entry1_proto.metrics()));

  EXPECT_EQ(entry2_proto.source_id(), source2->id());
  EXPECT_EQ(entry2_proto.event_hash(),
            base::HashMetricName(internal::kUkmPageLoadEventName));
  EXPECT_FALSE(entry2_proto.metrics().empty());
  EXPECT_FALSE(HasMetric(internal::kUkmParseStartName, entry2_proto.metrics()));
  EXPECT_FALSE(HasMetric(internal::kUkmFirstContentfulPaintName,
                         entry2_proto.metrics()));
  EXPECT_FALSE(HasMetric(internal::kUkmFirstMeaningfulPaintName,
                         entry2_proto.metrics()));
  EXPECT_TRUE(
      HasMetric(internal::kUkmForegroundDurationName, entry2_proto.metrics()));
}

TEST_F(UkmPageLoadMetricsObserverTest, EffectiveConnectionType) {
  EXPECT_CALL(mock_network_quality_provider(), GetEffectiveConnectionType())
      .WillRepeatedly(Return(net::EFFECTIVE_CONNECTION_TYPE_3G));

  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate closing the tab.
  DeleteContents();

  EXPECT_EQ(1ul, ukm_source_count());
  const ukm::UkmSource* source = GetUkmSourceForUrl(kTestUrl1);
  EXPECT_EQ(GURL(kTestUrl1), source->url());

  EXPECT_GE(ukm_entry_count(), 1ul);
  ukm::Entry entry_proto = GetMergedEntryProtoForSourceID(source->id());
  EXPECT_EQ(entry_proto.source_id(), source->id());
  EXPECT_EQ(entry_proto.event_hash(),
            base::HashMetricName(internal::kUkmPageLoadEventName));
  EXPECT_FALSE(entry_proto.metrics().empty());
  ExpectMetric(internal::kUkmEffectiveConnectionType,
               net::EFFECTIVE_CONNECTION_TYPE_3G, entry_proto.metrics());
}

TEST_F(UkmPageLoadMetricsObserverTest, PageTransitionReload) {
  GURL url(kTestUrl1);
  NavigateWithPageTransitionAndCommit(GURL(kTestUrl1),
                                      ui::PAGE_TRANSITION_RELOAD);

  // Simulate closing the tab.
  DeleteContents();

  EXPECT_EQ(1ul, ukm_source_count());
  const ukm::UkmSource* source = GetUkmSourceForUrl(kTestUrl1);
  EXPECT_EQ(GURL(kTestUrl1), source->url());

  EXPECT_GE(ukm_entry_count(), 1ul);
  ukm::Entry entry_proto = GetMergedEntryProtoForSourceID(source->id());
  EXPECT_EQ(entry_proto.source_id(), source->id());
  EXPECT_EQ(entry_proto.event_hash(),
            base::HashMetricName(internal::kUkmPageLoadEventName));
  EXPECT_FALSE(entry_proto.metrics().empty());
  ExpectMetric(internal::kUkmPageTransition, ui::PAGE_TRANSITION_RELOAD,
               entry_proto.metrics());
}
