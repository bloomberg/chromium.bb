// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/noscript_preview_page_load_metrics_observer.h"

#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "chrome/common/page_load_metrics/test/page_load_metrics_test_util.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_user_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/previews_state.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"

namespace previews {

namespace {

using page_load_metrics::mojom::ResourceDataUpdate;
using page_load_metrics::mojom::ResourceDataUpdatePtr;

const char kDefaultTestUrl[] = "https://www.google.com";

class TestNoScriptPreviewPageLoadMetricsObserver
    : public NoScriptPreviewPageLoadMetricsObserver {
 public:
  TestNoScriptPreviewPageLoadMetricsObserver(
      base::OnceCallback<void(const GURL&, int64_t)> bytes_callback)
      : bytes_callback_(std::move(bytes_callback)) {}
  ~TestNoScriptPreviewPageLoadMetricsObserver() override {}

  void WriteToSavings(const GURL& url, int64_t bytes_savings) override {
    std::move(bytes_callback_).Run(url, bytes_savings);
  }

 private:
  base::OnceCallback<void(const GURL&, int64_t)> bytes_callback_;
};

class NoScriptPreviewPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  NoScriptPreviewPageLoadMetricsObserverTest() {}
  ~NoScriptPreviewPageLoadMetricsObserverTest() override {}

  void ResetTest() {
    page_load_metrics::InitPageLoadTimingForTest(&timing_);
    // Reset to the default testing state. Does not reset histogram state.
    timing_.navigation_start = base::Time::FromDoubleT(1);
    timing_.response_start = base::TimeDelta::FromSeconds(2);
    timing_.parse_timing->parse_start = base::TimeDelta::FromSeconds(3);
    timing_.paint_timing->first_contentful_paint =
        base::TimeDelta::FromSeconds(4);
    timing_.paint_timing->first_paint = base::TimeDelta::FromSeconds(4);
    timing_.paint_timing->first_meaningful_paint =
        base::TimeDelta::FromSeconds(8);
    timing_.paint_timing->first_image_paint = base::TimeDelta::FromSeconds(5);
    timing_.paint_timing->first_text_paint = base::TimeDelta::FromSeconds(6);
    timing_.document_timing->load_event_start = base::TimeDelta::FromSeconds(7);
    timing_.parse_timing->parse_stop = base::TimeDelta::FromSeconds(4);
    timing_.parse_timing->parse_blocked_on_script_load_duration =
        base::TimeDelta::FromSeconds(1);
    PopulateRequiredTimingFields(&timing_);
  }

  content::GlobalRequestID NavigateAndCommitWithPreviewsState(
      content::PreviewsState previews_state) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(
            GURL(kDefaultTestUrl), main_rfh());
    navigation_simulator->Start();
    auto chrome_navigation_data = std::make_unique<ChromeNavigationData>();
    chrome_navigation_data->set_previews_state(previews_state);

    auto data = std::make_unique<previews::PreviewsUserData>(1);
    chrome_navigation_data->set_previews_user_data(std::move(data));

    content::WebContentsTester::For(web_contents())
        ->SetNavigationData(navigation_simulator->GetNavigationHandle(),
                            std::move(chrome_navigation_data));
    navigation_simulator->Commit();
    return navigation_simulator->GetGlobalRequestID();
  }

  void ValidateTimingHistograms(bool noscript_preview_request_sent) {
    ValidateTimingHistogram(noscript_preview_names::kNavigationToLoadEvent,
                            timing_.document_timing->load_event_start,
                            noscript_preview_request_sent);
    ValidateTimingHistogram(
        noscript_preview_names::kNavigationToFirstContentfulPaint,
        timing_.paint_timing->first_contentful_paint,
        noscript_preview_request_sent);
    ValidateTimingHistogram(
        noscript_preview_names::kNavigationToFirstMeaningfulPaint,
        timing_.paint_timing->first_meaningful_paint,
        noscript_preview_request_sent);
    ValidateTimingHistogram(
        noscript_preview_names::kParseBlockedOnScriptLoad,
        timing_.parse_timing->parse_blocked_on_script_load_duration,
        noscript_preview_request_sent);
    ValidateTimingHistogram(noscript_preview_names::kParseDuration,
                            timing_.parse_timing->parse_stop.value() -
                                timing_.parse_timing->parse_start.value(),
                            noscript_preview_request_sent);
  }

  void ValidateTimingHistogram(const std::string& histogram,
                               const base::Optional<base::TimeDelta>& event,
                               bool noscript_preview_request_sent) {
    histogram_tester().ExpectTotalCount(histogram,
                                        noscript_preview_request_sent ? 1 : 0);
    if (!noscript_preview_request_sent) {
      histogram_tester().ExpectTotalCount(histogram, 0);
    } else {
      histogram_tester().ExpectUniqueSample(
          histogram,
          static_cast<base::HistogramBase::Sample>(
              event.value().InMilliseconds()),
          1);
    }
  }

  void ValidateDataHistograms(int network_resources, int64_t network_bytes) {
    if (network_resources > 0) {
      histogram_tester().ExpectUniqueSample(
          noscript_preview_names::kNumNetworkResources, network_resources, 1);
      histogram_tester().ExpectUniqueSample(
          noscript_preview_names::kNetworkBytes,
          static_cast<int>(network_bytes / 1024), 1);
    } else {
      histogram_tester().ExpectTotalCount(
          noscript_preview_names::kNumNetworkResources, 0);
      histogram_tester().ExpectTotalCount(noscript_preview_names::kNetworkBytes,
                                          0);
    }
  }

  void WriteToSavings(const GURL& url, int64_t bytes_savings) {
    savings_url_ = url;
    bytes_savings_ = bytes_savings;
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<TestNoScriptPreviewPageLoadMetricsObserver>(
            base::BindOnce(
                &NoScriptPreviewPageLoadMetricsObserverTest::WriteToSavings,
                base::Unretained(this))));
  }

  page_load_metrics::mojom::PageLoadTiming timing_;

  GURL savings_url_;
  int64_t bytes_savings_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NoScriptPreviewPageLoadMetricsObserverTest);
};

TEST_F(NoScriptPreviewPageLoadMetricsObserverTest, NoScriptPreviewNotSeen) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      previews::features::kNoScriptPreviews);
  ResetTest();

  content::GlobalRequestID request_id =
      NavigateAndCommitWithPreviewsState(content::PREVIEWS_OFF);

  page_load_metrics::ExtraRequestCompleteInfo main_frame_info(
      GURL(kResourceUrl), net::HostPortPair(), -1 /* frame_tree_node_id */,
      false, /* cached */
      5 * 1024 /* size */, 0 /* original_network_content_length */,
      nullptr
      /* data_reduction_proxy_data */,
      content::RESOURCE_TYPE_MAIN_FRAME, 0, nullptr /* load_timing_info */);
  SimulateLoadedResource(main_frame_info, request_id);

  page_load_metrics::ExtraRequestCompleteInfo network_resource_info(
      GURL(kResourceUrl), net::HostPortPair(), -1 /* frame_tree_node_id */,
      false, /* cached */
      20 * 1024 /* size */, 0 /* original_network_content_length */,
      nullptr
      /* data_reduction_proxy_data */,
      content::RESOURCE_TYPE_IMAGE, 0, nullptr /* load_timing_info */);
  SimulateLoadedResource(network_resource_info);

  SimulateTimingUpdate(timing_);
  NavigateToUntrackedUrl();

  ValidateTimingHistograms(false /* is_using_noscript */);
  ValidateDataHistograms(0 /* network_resources */, 0 /* network_bytes */);
}

TEST_F(NoScriptPreviewPageLoadMetricsObserverTest, NoScriptPreviewSeen) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      previews::features::kNoScriptPreviews);
  ResetTest();

  content::GlobalRequestID request_id =
      NavigateAndCommitWithPreviewsState(content::NOSCRIPT_ON);

  page_load_metrics::ExtraRequestCompleteInfo main_frame_info(
      GURL(kResourceUrl), net::HostPortPair(), -1 /* frame_tree_node_id */,
      false, /* cached */
      5 * 1024 /* size */, 0 /* original_network_content_length */,
      nullptr
      /* data_reduction_proxy_data */,
      content::RESOURCE_TYPE_MAIN_FRAME, 0, nullptr /* load_timing_info */);
  SimulateLoadedResource(main_frame_info, request_id);

  page_load_metrics::ExtraRequestCompleteInfo cached_resource_info(
      GURL(kResourceUrl), net::HostPortPair(), -1 /* frame_tree_node_id */,
      true, /* cached */
      13 * 1024 /* size */, 0 /* original_network_content_length */,
      nullptr
      /* data_reduction_proxy_data */,
      content::RESOURCE_TYPE_IMAGE, 0, nullptr /* load_timing_info */);
  SimulateLoadedResource(cached_resource_info);

  page_load_metrics::ExtraRequestCompleteInfo network_resource_info(
      GURL(kResourceUrl), net::HostPortPair(), -1 /* frame_tree_node_id */,
      false, /* cached */
      20 * 1024 /* size */, 0 /* original_network_content_length */,
      nullptr
      /* data_reduction_proxy_data */,
      content::RESOURCE_TYPE_IMAGE, 0, nullptr /* load_timing_info */);
  SimulateLoadedResource(network_resource_info);

  SimulateTimingUpdate(timing_);
  NavigateToUntrackedUrl();

  ValidateTimingHistograms(true /* is_using_noscript */);
  ValidateDataHistograms(2 /* network_resources */,
                         25 * 1024 /* network_bytes */);
}

TEST_F(NoScriptPreviewPageLoadMetricsObserverTest, DataSavings) {
  int inflation = 50;
  int constant_savings = 120;
  base::test::ScopedFeatureList scoped_feature_list;

  std::map<std::string, std::string> parameters = {
      {"NoScriptInflationPercent", base::IntToString(inflation)},
      {"NoScriptInflationBytes", base::IntToString(constant_savings)}};
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      previews::features::kNoScriptPreviews, parameters);

  ResetTest();

  int64_t data_use = 0;
  NavigateAndCommitWithPreviewsState(content::NOSCRIPT_ON);
  std::vector<ResourceDataUpdatePtr> resources;
  auto resource_data_update = ResourceDataUpdate::New();
  resource_data_update->delta_bytes = 5 * 1024;
  resources.push_back(std::move(resource_data_update));
  SimulateResourceDataUseUpdate(resources);
  data_use += (5 * 1024);

  resources.clear();

  resource_data_update = ResourceDataUpdate::New();
  resource_data_update->delta_bytes = 20 * 1024;
  resources.push_back(std::move(resource_data_update));
  SimulateResourceDataUseUpdate(resources);
  data_use += (20 * 1024);

  SimulateTimingUpdate(timing_);

  int64_t expected_savings = (data_use * inflation) / 100 + constant_savings;

  EXPECT_EQ(GURL(kDefaultTestUrl), savings_url_);
  EXPECT_EQ(expected_savings, bytes_savings_);
}

}  // namespace

}  //  namespace previews
