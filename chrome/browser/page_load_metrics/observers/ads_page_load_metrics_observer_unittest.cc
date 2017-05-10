// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ads_page_load_metrics_observer.h"

#include <string>

#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "url/gurl.h"

using content::RenderFrameHost;
using content::RenderFrameHostTester;
using content::NavigationSimulator;

namespace {

enum class ResourceCached { NOT_CACHED, CACHED };
enum class FrameType { AD = 0, NON_AD };

const char kAdUrl[] = "https://tpc.googlesyndication.com/safeframe/1";
const char kNonAdUrl[] = "https://foo.com/";
const char kNonAdUrl2[] = "https://bar.com/";

const char kAdName[] = "google_ads_iframe_1";
const char kNonAdName[] = "foo";

}  // namespace

class AdsPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  AdsPageLoadMetricsObserverTest() {}

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateFrame(const std::string& url,
                                 content::RenderFrameHost* frame) {
    auto navigation_simulator =
        NavigationSimulator::CreateRendererInitiated(GURL(url), frame);
    navigation_simulator->Commit();
    return navigation_simulator->GetFinalRenderFrameHost();
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* NavigateMainFrame(const std::string& url) {
    return NavigateFrame(url, web_contents()->GetMainFrame());
  }

  // Returns the final RenderFrameHost after navigation commits.
  RenderFrameHost* CreateAndNavigateSubFrame(const std::string& url,
                                             const std::string& frame_name,
                                             content::RenderFrameHost* parent) {
    RenderFrameHost* subframe =
        RenderFrameHostTester::For(parent)->AppendChild(frame_name);
    auto navigation_simulator =
        NavigationSimulator::CreateRendererInitiated(GURL(url), subframe);
    navigation_simulator->Commit();
    return navigation_simulator->GetFinalRenderFrameHost();
  }

  RenderFrameHost* NavigateFrameAndTestRenavigationMetrics(
      RenderFrameHost* frame,
      FrameType frame_type,
      const std::string& url) {
    base::HistogramTester tester;
    RenderFrameHost* out_frame = NavigateFrame(url, frame);

    int bucket = url == kAdUrl ? 1 : 0;

    if (frame_type == FrameType::AD) {
      tester.ExpectUniqueSample(
          "PageLoad.Clients.Ads.Google.Navigations.AdFrameRenavigatedToAd",
          bucket, 1);
    } else {
      tester.ExpectUniqueSample(
          "PageLoad.Clients.Ads.Google.Navigations.NonAdFrameRenavigatedToAd",
          bucket, 1);
    }

    return out_frame;
  }

  void LoadResource(RenderFrameHost* frame,
                    ResourceCached resource_cached,
                    int resource_size_in_kb) {
    page_load_metrics::ExtraRequestCompleteInfo request(
        GURL(kNonAdUrl), frame->GetFrameTreeNodeId(),
        resource_cached == ResourceCached::CACHED, resource_size_in_kb * 1024,
        0,       /* original_network_content_length */
        nullptr, /* data_reduction_proxy_data */
        content::RESOURCE_TYPE_SUB_FRAME);
    SimulateLoadedResource(request);
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::MakeUnique<AdsPageLoadMetricsObserver>());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AdsPageLoadMetricsObserverTest);
};

TEST_F(AdsPageLoadMetricsObserverTest, PageWithNoAds) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* frame1 =
      CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);
  RenderFrameHost* frame2 =
      CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);
  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);
  LoadResource(frame1, ResourceCached::NOT_CACHED, 10);
  LoadResource(frame2, ResourceCached::NOT_CACHED, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.AnyParentFrame.AdFrames", 0, 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Total", 0);
}

TEST_F(AdsPageLoadMetricsObserverTest, ResourceBeforeAdFrameCommits) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);

  // Assume that the next frame's id will be the main frame + 1 and load a
  // resource for that frame. Make sure it gets counted.
  page_load_metrics::ExtraRequestCompleteInfo request(
      GURL(kNonAdUrl), main_frame->GetFrameTreeNodeId() + 1, false /* cached */,
      10 * 1024 /* size */, 0 /* original_network_content_length */,
      nullptr
      /* data_reduction_proxy_data */,
      content::RESOURCE_TYPE_SUB_FRAME);
  SimulateLoadedResource(request);

  CreateAndNavigateSubFrame(kNonAdUrl, kAdName, main_frame);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  // 20KB total were loaded from network, one of which was in an ad frame.
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.AnyParentFrame.AdFrames", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.TotalFrames", 1,
      1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.AdFrames", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.PercentAdFrames",
      100, 1);

  // Individual Ad Frame Metrics
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Total", 10, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Network", 10, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.PercentNetwork", 100,
      1);

  // Page percentages
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Total.PercentAds", 50, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.PercentNetwork",
      100, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Network.PercentAds", 50, 1);

  // Page byte counts
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Total", 10, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Network", 10, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Total", 20, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Network", 20, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.NonAdFrames.Aggregate.Total", 10, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest, PageWithAdFrames) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* non_ad_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);

  // Create 5 ad frames with the 5th nested inside the 4th. Verify that the
  // nested ad frame doesn't get counted separately (but that its bytes are
  // still counted). Also verify that the various ad signals (urls and names)
  // are properly detected.
  RenderFrameHost* ad_frame1 =
      CreateAndNavigateSubFrame(kNonAdUrl, "google_ads_iframe_1", main_frame);
  RenderFrameHost* ad_frame2 =
      CreateAndNavigateSubFrame(kNonAdUrl, "google_ads_frame_2", main_frame);
  RenderFrameHost* ad_frame3 = CreateAndNavigateSubFrame(
      "https://tpc.googlesyndication.com/safeframe/", "", main_frame);
  RenderFrameHost* ad_frame4 = CreateAndNavigateSubFrame(
      "https://tpc.googlesyndication.com/safeframe/1", "", main_frame);
  RenderFrameHost* nested_ad_frame4 = CreateAndNavigateSubFrame(
      "https://tpc.googlesyndication.com/safeframe/2", "", ad_frame4);

  // Create an addditional ad frame without content, it shouldn't be counted
  // in some percentage calculations.
  CreateAndNavigateSubFrame(kAdUrl, kNonAdName, main_frame);

  // 70KB total in page, 50 from ads, 40 from network, and 30 of those
  // are from ads.
  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);
  LoadResource(non_ad_frame, ResourceCached::CACHED, 10);
  LoadResource(ad_frame1, ResourceCached::CACHED, 10);
  LoadResource(ad_frame2, ResourceCached::NOT_CACHED, 10);
  LoadResource(ad_frame3, ResourceCached::NOT_CACHED, 10);
  LoadResource(ad_frame4, ResourceCached::NOT_CACHED, 10);
  LoadResource(nested_ad_frame4, ResourceCached::CACHED, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  // Individual Ad Frame Metrics
  histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Total", 10, 3);
  histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Total", 20, 1);
  histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Network", 10, 3);
  histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Network", 0, 2);
  histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.PercentNetwork", 0,
      1);
  histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.PercentNetwork", 100,
      2);
  histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.PercentNetwork", 50,
      1);

  // Counts
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.AnyParentFrame.AdFrames", 5, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.TotalFrames", 6,
      1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.AdFrames", 5, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.PercentAdFrames",
      83, 1);

  // Page percentages
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Total.PercentAds", 71, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.PercentNetwork", 60,
      1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Network.PercentAds", 75, 1);

  // Page byte counts
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Total", 50, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Network", 30, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Total", 70, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Network", 40, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.NonAdFrames.Aggregate.Total", 20, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest, PageLoadSubFrameRenavigationMetrics) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  // Test behavior of an ad frame. Once a frame is considered an ad, it's
  // always an ad.
  RenderFrameHost* ad_sub_frame =
      CreateAndNavigateSubFrame(kAdUrl, kNonAdName, main_frame);

  ad_sub_frame = NavigateFrameAndTestRenavigationMetrics(ad_sub_frame,
                                                         FrameType::AD, kAdUrl);
  ad_sub_frame = NavigateFrameAndTestRenavigationMetrics(
      ad_sub_frame, FrameType::AD, kNonAdUrl);
  ad_sub_frame = NavigateFrameAndTestRenavigationMetrics(
      ad_sub_frame, FrameType::AD, kNonAdUrl);

  // Test behavior of a non-ad frame. Again, once it becomes an ad it stays an
  // ad.
  RenderFrameHost* non_ad_sub_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);
  non_ad_sub_frame = NavigateFrameAndTestRenavigationMetrics(
      non_ad_sub_frame, FrameType::NON_AD, kNonAdUrl);
  non_ad_sub_frame = NavigateFrameAndTestRenavigationMetrics(
      non_ad_sub_frame, FrameType::NON_AD, kAdUrl);
  non_ad_sub_frame = NavigateFrameAndTestRenavigationMetrics(
      non_ad_sub_frame, FrameType::AD, kNonAdUrl);
  non_ad_sub_frame = NavigateFrameAndTestRenavigationMetrics(
      non_ad_sub_frame, FrameType::AD, kAdUrl);

  // Verify that children of ad frames don't get counted in the renavigation
  // metrics.
  RenderFrameHost* ad_sub_sub_frame =
      CreateAndNavigateSubFrame(kAdUrl, kNonAdName, ad_sub_frame);
  base::HistogramTester tester;
  ad_sub_sub_frame = NavigateFrame(kNonAdUrl2, ad_sub_sub_frame);
  tester.ExpectTotalCount(
      "PageLoad.Clients.Ads.Google.Navigations.AdFrameRenavigatedToAd", 0);
}

TEST_F(AdsPageLoadMetricsObserverTest, PageWithAdFrameThatRenavigates) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  RenderFrameHost* ad_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, kAdName, main_frame);

  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);
  LoadResource(ad_frame, ResourceCached::NOT_CACHED, 10);

  // Navigate the ad frame again.
  ad_frame = NavigateFrame(kNonAdUrl, ad_frame);

  // In total, 30KB for entire page and 20 in one ad frame.
  LoadResource(ad_frame, ResourceCached::NOT_CACHED, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  // Individual Ad Frame Metrics
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Total", 20, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Network", 20, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.PercentNetwork", 100,
      1);

  // Counts
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.AnyParentFrame.AdFrames", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.TotalFrames", 1,
      1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.AdFrames", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.PercentAdFrames",
      100, 1);

  // Page percentages
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Total.PercentAds", 66, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.PercentNetwork",
      100, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Network.PercentAds", 66, 1);

  // Page byte counts
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Total", 20, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Network", 20, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Total", 30, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Network", 30, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.NonAdFrames.Aggregate.Total", 10, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest, PageWithNonAdFrameThatRenavigatesToAd) {
  // Main frame.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);

  // Sub frame that is not an ad.
  RenderFrameHost* sub_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);

  // Child of the sub-frame that is an ad.
  RenderFrameHost* sub_frame_child_ad =
      CreateAndNavigateSubFrame(kNonAdUrl2, kAdName, sub_frame);

  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);
  LoadResource(sub_frame, ResourceCached::NOT_CACHED, 10);
  LoadResource(sub_frame_child_ad, ResourceCached::NOT_CACHED, 10);

  // Navigate the subframe again, this time it's an ad.
  sub_frame = NavigateFrame(kAdUrl, sub_frame);
  LoadResource(sub_frame, ResourceCached::NOT_CACHED, 10);

  // In total, 40KB was loaded for the entire page and 20KB from ad
  // frames (the original child ad frame and the renavigated frame which
  // turned into an ad).

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  // Individual Ad Frame Metrics
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Total", 10, 2);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Network", 10, 2);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.PercentNetwork", 100,
      2);

  // Counts
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.AnyParentFrame.AdFrames", 2, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.TotalFrames", 1,
      1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.AdFrames", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.PercentAdFrames",
      100, 1);

  // Page percentages
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Total.PercentAds", 50, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.PercentNetwork",
      100, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Network.PercentAds", 50, 1);

  // Page byte counts
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Total", 20, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Network", 20, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Total", 40, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Network", 40, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.NonAdFrames.Aggregate.Total", 20, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest, CountAbortedNavigation) {
  // If the first navigation in a frame is aborted, keep track of its bytes.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);

  // Create an ad subframe that aborts before committing.
  RenderFrameHost* subframe_ad =
      RenderFrameHostTester::For(main_frame)->AppendChild(kAdName);
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL(kNonAdUrl), subframe_ad);
  // The sub-frame renavigates before it commits.
  navigation_simulator->Start();
  navigation_simulator->Fail(net::ERR_ABORTED);

  // Load resources for the aborted frame (e.g., simulate the navigation
  // aborting due to a doc.write during provisional navigation). They should
  // be counted.
  LoadResource(subframe_ad, ResourceCached::NOT_CACHED, 10);
  LoadResource(subframe_ad, ResourceCached::NOT_CACHED, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.AnyParentFrame.AdFrames", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Total", 20, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Total", 30, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest, CountAbortedSecondNavigationForFrame) {
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);

  // Sub frame that is not an ad.
  RenderFrameHost* sub_frame =
      CreateAndNavigateSubFrame(kNonAdUrl, kNonAdName, main_frame);
  LoadResource(sub_frame, ResourceCached::NOT_CACHED, 10);

  // Now navigate (and abort) the subframe to an ad.
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(GURL(kAdUrl), sub_frame);
  // The sub-frame renavigates before it commits.
  navigation_simulator->Start();
  navigation_simulator->Fail(net::ERR_ABORTED);

  // Load resources for the aborted frame (e.g., simulate the navigation
  // aborting due to a doc.write during provisional navigation). Since the
  // frame attempted to load an ad, the frame is tagged forever as an ad.
  LoadResource(sub_frame, ResourceCached::NOT_CACHED, 10);
  LoadResource(sub_frame, ResourceCached::NOT_CACHED, 10);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.AnyParentFrame.AdFrames", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Total", 20, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Total", 40, 1);
}

TEST_F(AdsPageLoadMetricsObserverTest, TwoResourceLoadsBeforeCommit) {
  // Main frame.
  RenderFrameHost* main_frame = NavigateMainFrame(kNonAdUrl);
  LoadResource(main_frame, ResourceCached::NOT_CACHED, 10);

  // Now open a subframe and have its resource load before notification of
  // navigation finishing.
  page_load_metrics::ExtraRequestCompleteInfo request(
      GURL(kNonAdUrl), main_frame->GetFrameTreeNodeId() + 1, false /* cached */,
      10 * 1024 /* size */, false /* data_reduction_proxy_used */,
      0 /* original_network_content_length */,
      content::RESOURCE_TYPE_SUB_FRAME);
  SimulateLoadedResource(request);
  RenderFrameHost* subframe_ad =
      RenderFrameHostTester::For(main_frame)->AppendChild(kAdName);
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL(kNonAdUrl), subframe_ad);

  // The sub-frame renavigates before it commits.
  navigation_simulator->Start();
  navigation_simulator->Fail(net::ERR_ABORTED);

  // Renavigate the subframe to a successful commit. But again, the resource
  // loads before the observer sees the finished navigation.
  SimulateLoadedResource(request);
  NavigateFrame(kNonAdUrl, subframe_ad);

  // Navigate again to trigger histograms.
  NavigateFrame(kNonAdUrl, main_frame);

  // 30KB in total was loaded. Ten for the main page, ten for an aborted
  // ad subframe, and ten for a successful ad subframe. The aborted ad
  // subframe's bytes count.

  // Individual Ad Frame Metrics
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Total", 20, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Network", 20, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.PercentNetwork", 100,
      1);

  // Counts
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.AnyParentFrame.AdFrames", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.TotalFrames", 1,
      1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.AdFrames", 1, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.PercentAdFrames",
      100, 1);

  // Page percentages
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Total.PercentAds", 66, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.PercentNetwork",
      100, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Network.PercentAds", 66, 1);

  // Page byte counts
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Total", 20, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Network", 20, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Total", 30, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.FullPage.Network", 30, 1);
  histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.Ads.Google.Bytes.NonAdFrames.Aggregate.Total", 10, 1);
}
