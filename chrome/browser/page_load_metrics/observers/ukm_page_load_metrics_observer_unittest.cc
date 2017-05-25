// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ukm_page_load_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/metrics_hashes.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/ukm/test_ukm_recorder.h"
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
  MOCK_CONST_METHOD0(GetHttpRTT, base::Optional<base::TimeDelta>());
  MOCK_CONST_METHOD0(GetTransportRTT, base::Optional<base::TimeDelta>());
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

    EXPECT_CALL(mock_network_quality_provider_, GetHttpRTT())
        .Times(AnyNumber())
        .WillRepeatedly(Return(base::Optional<base::TimeDelta>()));

    EXPECT_CALL(mock_network_quality_provider_, GetTransportRTT())
        .Times(AnyNumber())
        .WillRepeatedly(Return(base::Optional<base::TimeDelta>()));

    TestingBrowserProcess::GetGlobal()->SetUkmRecorder(&test_ukm_recorder_);
  }

  size_t ukm_source_count() { return test_ukm_recorder_.sources_count(); }

  size_t ukm_entry_count() { return test_ukm_recorder_.entries_count(); }

  MockNetworkQualityProvider& mock_network_quality_provider() {
    return mock_network_quality_provider_;
  }

  const ukm::UkmSource* GetUkmSourceForUrl(const char* url) {
    return test_ukm_recorder_.GetSourceForUrl(url);
  }

  const ukm::mojom::UkmEntry* GetUkmEntry(size_t entry_index) {
    return test_ukm_recorder_.GetEntry(entry_index);
  }

  std::vector<const ukm::mojom::UkmEntry*> GetUkmEntriesForSourceID(
      ukm::SourceId source_id) {
    std::vector<const ukm::mojom::UkmEntry*> entries;
    for (size_t i = 0; i < ukm_entry_count(); ++i) {
      const ukm::mojom::UkmEntry* entry = GetUkmEntry(i);
      if (entry->source_id == source_id)
        entries.push_back(entry);
    }
    return entries;
  }

  // Provides a single merged ukm::mojom::UkmEntry proto that contains all
  // metrics from the given |entries|. |entries| must be non-empty, and all
  // |entries| must have the same |source_id| and |event_hash|.
  ukm::mojom::UkmEntryPtr GetMergedEntry(
      const std::vector<const ukm::mojom::UkmEntry*>& entries) {
    EXPECT_FALSE(entries.empty());
    ukm::mojom::UkmEntryPtr merged_entry = ukm::mojom::UkmEntry::New();
    for (const auto* entry : entries) {
      if (merged_entry->event_hash) {
        EXPECT_EQ(merged_entry->source_id, entry->source_id);
        EXPECT_EQ(merged_entry->event_hash, entry->event_hash);
      } else {
        merged_entry->event_hash = entry->event_hash;
        merged_entry->source_id = entry->source_id;
      }
      for (const auto& metric : entry->metrics) {
        merged_entry->metrics.emplace_back(metric->Clone());
      }
    }
    return merged_entry;
  }

  ukm::mojom::UkmEntryPtr GetMergedEntryForSourceID(ukm::SourceId source_id) {
    ukm::mojom::UkmEntryPtr entry =
        GetMergedEntry(GetUkmEntriesForSourceID(source_id));
    EXPECT_EQ(source_id, entry->source_id);
    EXPECT_NE(0UL, entry->event_hash);
    return entry;
  }

  static bool HasMetric(const char* name,
                        const ukm::mojom::UkmEntry* entry) WARN_UNUSED_RESULT {
    return ukm::TestUkmRecorder::FindMetric(entry, name) != nullptr;
  }

  static void ExpectMetric(const char* name,
                           int64_t expected_value,
                           const ukm::mojom::UkmEntry* entry) {
    const ukm::mojom::UkmMetric* metric =
        ukm::TestUkmRecorder::FindMetric(entry, name);
    EXPECT_NE(nullptr, metric) << "Failed to find metric: " << name;
    EXPECT_EQ(expected_value, metric->value);
  }

 private:
  MockNetworkQualityProvider mock_network_quality_provider_;
  ukm::TestUkmRecorder test_ukm_recorder_;
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
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(100);
  timing.document_timing->dom_content_loaded_event_start =
      base::TimeDelta::FromMilliseconds(200);
  timing.paint_timing->first_paint = base::TimeDelta::FromMilliseconds(250);
  timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(300);
  timing.document_timing->load_event_start =
      base::TimeDelta::FromMilliseconds(500);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  EXPECT_EQ(1ul, ukm_source_count());
  const ukm::UkmSource* source = GetUkmSourceForUrl(kTestUrl1);
  EXPECT_EQ(GURL(kTestUrl1), source->url());

  EXPECT_GE(ukm_entry_count(), 1ul);
  ukm::mojom::UkmEntryPtr entry = GetMergedEntryForSourceID(source->id());
  EXPECT_EQ(entry->source_id, source->id());
  EXPECT_EQ(entry->event_hash,
            base::HashMetricName(internal::kUkmPageLoadEventName));
  EXPECT_FALSE(entry->metrics.empty());
  ExpectMetric(internal::kUkmPageTransition, ui::PAGE_TRANSITION_LINK,
               entry.get());
  ExpectMetric(internal::kUkmParseStartName, 100, entry.get());
  ExpectMetric(internal::kUkmDomContentLoadedName, 200, entry.get());
  ExpectMetric(internal::kUkmFirstPaintName, 250, entry.get());
  ExpectMetric(internal::kUkmFirstContentfulPaintName, 300, entry.get());
  ExpectMetric(internal::kUkmLoadEventName, 500, entry.get());
  EXPECT_FALSE(HasMetric(internal::kUkmFirstMeaningfulPaintName, entry.get()));
  EXPECT_TRUE(HasMetric(internal::kUkmForegroundDurationName, entry.get()));
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
  ukm::mojom::UkmEntryPtr entry = GetMergedEntryForSourceID(source->id());
  EXPECT_EQ(entry->source_id, source->id());
  EXPECT_EQ(entry->event_hash,
            base::HashMetricName(internal::kUkmPageLoadEventName));

  // Make sure that only the following metrics are logged. In particular, no
  // paint/document/etc timing metrics should be logged for failed provisional
  // loads.
  EXPECT_EQ(5ul, entry->metrics.size());
  ExpectMetric(internal::kUkmPageTransition, ui::PAGE_TRANSITION_LINK,
               entry.get());
  ExpectMetric(internal::kUkmEffectiveConnectionType,
               net::EFFECTIVE_CONNECTION_TYPE_2G, entry.get());
  ExpectMetric(internal::kUkmNetErrorCode,
               static_cast<int64_t>(net::ERR_TIMED_OUT) * -1, entry.get());
  EXPECT_TRUE(HasMetric(internal::kUkmForegroundDurationName, entry.get()));
  EXPECT_TRUE(HasMetric(internal::kUkmFailedProvisionaLoadName, entry.get()));
}

TEST_F(UkmPageLoadMetricsObserverTest, FirstMeaningfulPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_meaningful_paint =
      base::TimeDelta::FromMilliseconds(600);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  EXPECT_EQ(1ul, ukm_source_count());
  const ukm::UkmSource* source = GetUkmSourceForUrl(kTestUrl1);
  EXPECT_EQ(GURL(kTestUrl1), source->url());

  EXPECT_GE(ukm_entry_count(), 1ul);
  ukm::mojom::UkmEntryPtr entry = GetMergedEntryForSourceID(source->id());
  EXPECT_EQ(entry->source_id, source->id());
  EXPECT_EQ(entry->event_hash,
            base::HashMetricName(internal::kUkmPageLoadEventName));
  EXPECT_FALSE(entry->metrics.empty());
  ExpectMetric(internal::kUkmFirstMeaningfulPaintName, 600, entry.get());
  EXPECT_TRUE(HasMetric(internal::kUkmForegroundDurationName, entry.get()));
}

TEST_F(UkmPageLoadMetricsObserverTest, MultiplePageLoads) {
  page_load_metrics::mojom::PageLoadTiming timing1;
  page_load_metrics::InitPageLoadTimingForTest(&timing1);
  timing1.navigation_start = base::Time::FromDoubleT(1);
  timing1.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(200);
  PopulateRequiredTimingFields(&timing1);

  // Second navigation reports no timing metrics.
  page_load_metrics::mojom::PageLoadTiming timing2;
  page_load_metrics::InitPageLoadTimingForTest(&timing2);
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
  ukm::mojom::UkmEntryPtr entry1 = GetMergedEntryForSourceID(source1->id());
  ukm::mojom::UkmEntryPtr entry2 = GetMergedEntryForSourceID(source2->id());
  EXPECT_NE(entry1->source_id, entry2->source_id);

  EXPECT_EQ(entry1->source_id, source1->id());
  EXPECT_EQ(entry1->event_hash,
            base::HashMetricName(internal::kUkmPageLoadEventName));
  EXPECT_FALSE(entry1->metrics.empty());
  ExpectMetric(internal::kUkmFirstContentfulPaintName, 200, entry1.get());
  EXPECT_FALSE(HasMetric(internal::kUkmFirstMeaningfulPaintName, entry2.get()));
  EXPECT_TRUE(HasMetric(internal::kUkmForegroundDurationName, entry1.get()));

  EXPECT_EQ(entry2->source_id, source2->id());
  EXPECT_EQ(entry2->event_hash,
            base::HashMetricName(internal::kUkmPageLoadEventName));
  EXPECT_FALSE(entry2->metrics.empty());
  EXPECT_FALSE(HasMetric(internal::kUkmParseStartName, entry2.get()));
  EXPECT_FALSE(HasMetric(internal::kUkmFirstContentfulPaintName, entry2.get()));
  EXPECT_FALSE(HasMetric(internal::kUkmFirstMeaningfulPaintName, entry2.get()));
  EXPECT_TRUE(HasMetric(internal::kUkmForegroundDurationName, entry2.get()));
}

TEST_F(UkmPageLoadMetricsObserverTest, NetworkQualityEstimates) {
  EXPECT_CALL(mock_network_quality_provider(), GetEffectiveConnectionType())
      .WillRepeatedly(Return(net::EFFECTIVE_CONNECTION_TYPE_3G));
  EXPECT_CALL(mock_network_quality_provider(), GetHttpRTT())
      .WillRepeatedly(Return(base::TimeDelta::FromMilliseconds(100)));
  EXPECT_CALL(mock_network_quality_provider(), GetTransportRTT())
      .WillRepeatedly(Return(base::TimeDelta::FromMilliseconds(200)));

  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate closing the tab.
  DeleteContents();

  EXPECT_EQ(1ul, ukm_source_count());
  const ukm::UkmSource* source = GetUkmSourceForUrl(kTestUrl1);
  EXPECT_EQ(GURL(kTestUrl1), source->url());

  EXPECT_GE(ukm_entry_count(), 1ul);
  ukm::mojom::UkmEntryPtr entry = GetMergedEntryForSourceID(source->id());
  EXPECT_EQ(entry->source_id, source->id());
  EXPECT_EQ(entry->event_hash,
            base::HashMetricName(internal::kUkmPageLoadEventName));
  EXPECT_FALSE(entry->metrics.empty());
  ExpectMetric(internal::kUkmEffectiveConnectionType,
               net::EFFECTIVE_CONNECTION_TYPE_3G, entry.get());
  ExpectMetric(internal::kUkmHttpRttEstimate, 100, entry.get());
  ExpectMetric(internal::kUkmTransportRttEstimate, 200, entry.get());
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
  ukm::mojom::UkmEntryPtr entry = GetMergedEntryForSourceID(source->id());
  EXPECT_EQ(entry->source_id, source->id());
  EXPECT_EQ(entry->event_hash,
            base::HashMetricName(internal::kUkmPageLoadEventName));
  EXPECT_FALSE(entry->metrics.empty());
  ExpectMetric(internal::kUkmPageTransition, ui::PAGE_TRANSITION_RELOAD,
               entry.get());
}
