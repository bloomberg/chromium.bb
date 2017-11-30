// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ukm_page_load_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/metrics_hashes.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/common/page_load_metrics/test/page_load_metrics_test_util.h"
#include "components/ukm/ukm_source.h"
#include "content/public/test/navigation_simulator.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/metrics_proto/system_profile.pb.h"

using testing::AnyNumber;
using testing::Mock;
using testing::Return;

namespace {

const char kTestUrl1[] = "https://www.google.com/";
const char kTestUrl2[] = "https://www.example.com/";

class MockNetworkQualityProvider : public net::NetworkQualityProvider {
 public:
  MOCK_CONST_METHOD0(GetEffectiveConnectionType,
                     net::EffectiveConnectionType());
  MOCK_CONST_METHOD0(GetHttpRTT, base::Optional<base::TimeDelta>());
  MOCK_CONST_METHOD0(GetTransportRTT, base::Optional<base::TimeDelta>());
  MOCK_CONST_METHOD0(GetDownstreamThroughputKbps, base::Optional<int32_t>());
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

    EXPECT_CALL(mock_network_quality_provider_, GetDownstreamThroughputKbps())
        .Times(AnyNumber())
        .WillRepeatedly(Return(base::Optional<int32_t>()));
  }

  MockNetworkQualityProvider& mock_network_quality_provider() {
    return mock_network_quality_provider_;
  }

 private:
  MockNetworkQualityProvider mock_network_quality_provider_;
};

TEST_F(UkmPageLoadMetricsObserverTest, NoMetrics) {
  EXPECT_EQ(0ul, test_ukm_recorder().sources_count());
  EXPECT_EQ(0ul, test_ukm_recorder().entries_count());
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

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(
          internal::kUkmPageLoadEventName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(kv.second.get(),
                                          internal::kUkmPageTransition,
                                          ui::PAGE_TRANSITION_LINK);
    test_ukm_recorder().ExpectEntryMetric(kv.second.get(),
                                          internal::kUkmParseStartName, 100);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), internal::kUkmDomContentLoadedName, 200);
    test_ukm_recorder().ExpectEntryMetric(kv.second.get(),
                                          internal::kUkmFirstPaintName, 250);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), internal::kUkmFirstContentfulPaintName, 300);
    test_ukm_recorder().ExpectEntryMetric(kv.second.get(),
                                          internal::kUkmLoadEventName, 500);
    EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), internal::kUkmFirstMeaningfulPaintName));
    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), internal::kUkmForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, FailedProvisionalLoad) {
  EXPECT_CALL(mock_network_quality_provider(), GetEffectiveConnectionType())
      .WillRepeatedly(Return(net::EFFECTIVE_CONNECTION_TYPE_2G));

  // The following simulates a navigation that fails and should commit an error
  // page, but finishes before the error page actually commits.
  GURL url(kTestUrl1);
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  navigation->Fail(net::ERR_TIMED_OUT);
  content::RenderFrameHostTester::For(main_rfh())->SimulateNavigationStop();

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(
          internal::kUkmPageLoadEventName);
  EXPECT_EQ(1ul, merged_entries.size());

  // Make sure that only the following metrics are logged. In particular, no
  // paint/document/etc timing metrics should be logged for failed provisional
  // loads.
  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(kv.second.get(),
                                          internal::kUkmPageTransition,
                                          ui::PAGE_TRANSITION_LINK);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), internal::kUkmEffectiveConnectionType,
        metrics::SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), internal::kUkmNetErrorCode,
        static_cast<int64_t>(net::ERR_TIMED_OUT) * -1);
    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), internal::kUkmForegroundDurationName));
    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), internal::kUkmFailedProvisionaLoadName));
  }
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

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(
          internal::kUkmPageLoadEventName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), internal::kUkmFirstMeaningfulPaintName, 600);
    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), internal::kUkmForegroundDurationName));
  }
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

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(
          internal::kUkmPageLoadEventName);
  EXPECT_EQ(2ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry1 = nullptr;
  const ukm::mojom::UkmEntry* entry2 = nullptr;
  for (const auto& kv : merged_entries) {
    if (test_ukm_recorder().EntryHasMetric(
            kv.second.get(), internal::kUkmFirstContentfulPaintName)) {
      entry1 = kv.second.get();
    } else {
      entry2 = kv.second.get();
    }
  }
  ASSERT_NE(entry1, nullptr);
  ASSERT_NE(entry2, nullptr);

  test_ukm_recorder().ExpectEntrySourceHasUrl(entry1, GURL(kTestUrl1));
  test_ukm_recorder().ExpectEntryMetric(
      entry1, internal::kUkmFirstContentfulPaintName, 200);
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entry1, internal::kUkmFirstMeaningfulPaintName));
  EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
      entry1, internal::kUkmForegroundDurationName));

  test_ukm_recorder().ExpectEntrySourceHasUrl(entry2, GURL(kTestUrl2));
  EXPECT_FALSE(
      test_ukm_recorder().EntryHasMetric(entry2, internal::kUkmParseStartName));
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entry2, internal::kUkmFirstContentfulPaintName));
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entry2, internal::kUkmFirstMeaningfulPaintName));
  EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
      entry2, internal::kUkmForegroundDurationName));
}

TEST_F(UkmPageLoadMetricsObserverTest, NetworkQualityEstimates) {
  EXPECT_CALL(mock_network_quality_provider(), GetEffectiveConnectionType())
      .WillRepeatedly(Return(net::EFFECTIVE_CONNECTION_TYPE_3G));
  EXPECT_CALL(mock_network_quality_provider(), GetHttpRTT())
      .WillRepeatedly(Return(base::TimeDelta::FromMilliseconds(100)));
  EXPECT_CALL(mock_network_quality_provider(), GetTransportRTT())
      .WillRepeatedly(Return(base::TimeDelta::FromMilliseconds(200)));
  EXPECT_CALL(mock_network_quality_provider(), GetDownstreamThroughputKbps())
      .WillRepeatedly(Return(300));

  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(
          internal::kUkmPageLoadEventName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), internal::kUkmEffectiveConnectionType,
        metrics::SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_3G);
    test_ukm_recorder().ExpectEntryMetric(kv.second.get(),
                                          internal::kUkmHttpRttEstimate, 100);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), internal::kUkmTransportRttEstimate, 200);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), internal::kUkmDownstreamKbpsEstimate, 300);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, PageTransitionReload) {
  GURL url(kTestUrl1);
  NavigateWithPageTransitionAndCommit(GURL(kTestUrl1),
                                      ui::PAGE_TRANSITION_RELOAD);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(
          internal::kUkmPageLoadEventName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(kv.second.get(),
                                          internal::kUkmPageTransition,
                                          ui::PAGE_TRANSITION_RELOAD);
  }
}
