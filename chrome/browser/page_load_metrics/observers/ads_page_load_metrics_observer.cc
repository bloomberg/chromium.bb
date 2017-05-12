// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ads_page_load_metrics_observer.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

const base::Feature kAdsFeature{"AdsMetrics",
                                base::FEATURE_DISABLED_BY_DEFAULT};

bool FrameIsAd(content::NavigationHandle* navigation_handle) {
  // Because sub-resource filtering isn't always enabled, and doesn't work
  // well in monitoring mode (no CSS enforcement), it's difficult to identify
  // ads. Google ads are prevalent and easy to track, so we'll start by
  // tracking those. Note that the frame name can be very large, so be careful
  // to avoid full string searches if possible.
  // TODO(jkarlin): Track other ad networks that are easy to identify.

  // In case the navigation aborted, look up the RFH by the Frame Tree Node
  // ID. It returns the committed frame host or the initial frame host for the
  // frame if no committed host exists. Using a previous host is fine because
  // once a frame has an ad we always consider it to have an ad.
  // We use the unsafe method of FindFrameByFrameTreeNodeId because we're not
  // concerned with which process the frame lives on (we're just measuring
  // bytes and not granting security priveleges).
  content::RenderFrameHost* current_frame_host =
      navigation_handle->GetWebContents()->UnsafeFindFrameByFrameTreeNodeId(
          navigation_handle->GetFrameTreeNodeId());
  if (current_frame_host) {
    const std::string& frame_name = current_frame_host->GetFrameName();
    if (base::StartsWith(frame_name, "google_ads_iframe",
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(frame_name, "google_ads_frame",
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }

  const GURL& url = navigation_handle->GetURL();
  return url.host_piece() == "tpc.googlesyndication.com" &&
         base::StartsWith(url.path_piece(), "/safeframe",
                          base::CompareCase::SENSITIVE);
}

}  // namespace

AdsPageLoadMetricsObserver::AdFrameData::AdFrameData(
    FrameTreeNodeId frame_tree_node_id)
    : frame_bytes(0u),
      frame_bytes_uncached(0u),
      frame_tree_node_id(frame_tree_node_id) {}

// static
std::unique_ptr<AdsPageLoadMetricsObserver>
AdsPageLoadMetricsObserver::CreateIfNeeded() {
  if (!base::FeatureList::IsEnabled(kAdsFeature))
    return nullptr;
  return base::MakeUnique<AdsPageLoadMetricsObserver>();
}

AdsPageLoadMetricsObserver::AdsPageLoadMetricsObserver() = default;
AdsPageLoadMetricsObserver::~AdsPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AdsPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  DCHECK(ad_frames_data_.empty());

  // The main frame is never considered an ad.
  ad_frames_data_[navigation_handle->GetFrameTreeNodeId()] = nullptr;
  ProcessOngoingNavigationResource(navigation_handle->GetFrameTreeNodeId());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AdsPageLoadMetricsObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  // Determine if the frame is part of an existing ad, the root of a new ad,
  // or a non-ad frame. Once a frame is labled as an ad, it is always
  // considered an ad, even if it navigates to a non-ad page. This function
  // labels all of a page's frames, even those that fail to commit.
  FrameTreeNodeId frame_tree_node_id = navigation_handle->GetFrameTreeNodeId();
  content::RenderFrameHost* parent_frame_host =
      navigation_handle->GetParentFrame();

  const auto& id_and_data = ad_frames_data_.find(frame_tree_node_id);
  if (id_and_data != ad_frames_data_.end()) {
    // An existing subframe is navigating again.
    if (id_and_data->second) {
      // The subframe was an ad to begin with, keep tracking it as an ad.
      ProcessOngoingNavigationResource(frame_tree_node_id);

      if (frame_tree_node_id == id_and_data->second->frame_tree_node_id) {
        // This is the top-most frame in the ad.
        UMA_HISTOGRAM_BOOLEAN(
            "PageLoad.Clients.Ads.Google.Navigations.AdFrameRenavigatedToAd",
            FrameIsAd(navigation_handle));
      }
      return CONTINUE_OBSERVING;
    }
    // This frame was previously not an ad, process it as usual. If it had
    // any child frames that were ads, those will still be recorded.
    UMA_HISTOGRAM_BOOLEAN(
        "PageLoad.Clients.Ads.Google.Navigations.NonAdFrameRenavigatedToAd",
        FrameIsAd(navigation_handle));
  } else if (navigation_handle->IsParentMainFrame()) {
    top_level_subframe_count_ += 1;
  }

  // Determine who the parent frame's ad ancestor is.
  const auto& parent_id_and_data =
      ad_frames_data_.find(parent_frame_host->GetFrameTreeNodeId());
  DCHECK(parent_id_and_data != ad_frames_data_.end());
  AdFrameData* ad_data = parent_id_and_data->second;

  if (!ad_data && FrameIsAd(navigation_handle)) {
    // This frame is not nested within an ad frame but is itself an ad.
    ad_frames_data_storage_.emplace_back(frame_tree_node_id);
    ad_data = &ad_frames_data_storage_.back();
  }

  ad_frames_data_[frame_tree_node_id] = ad_data;

  if (navigation_handle->IsParentMainFrame() && ad_data)
    top_level_ad_frame_count_ += 1;

  ProcessOngoingNavigationResource(frame_tree_node_id);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AdsPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  // The browser may come back, but there is no guarantee. To be safe, record
  // what we have now and ignore future changes to this navigation.
  if (extra_info.did_commit)
    RecordHistograms();

  return STOP_OBSERVING;
}

void AdsPageLoadMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo& extra_request_info) {
  ProcessLoadedResource(extra_request_info);
}

void AdsPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordHistograms();
}

void AdsPageLoadMetricsObserver::ProcessLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo& extra_request_info) {
  if (!extra_request_info.url.SchemeIsHTTPOrHTTPS()) {
    // Data uris should be accounted for in the generating resource, not
    // here. Blobs for PlzNavigate shouldn't be counted as the http resource
    // was already counted. Blobs for other things like CacheStorage or
    // IndexedDB are also ignored for now, as they're not normal HTTP loads.
    return;
  }

  const auto& id_and_data =
      ad_frames_data_.find(extra_request_info.frame_tree_node_id);
  if (id_and_data == ad_frames_data_.end()) {
    // This resource is for a frame that hasn't ever finished a navigation. We
    // expect it to be the frame's main resource but don't have enough
    // confidence in that to dcheck it. For example, there might be races
    // between doc.written resources and navigation failure.
    // TODO(jkarlin): Add UMA to measure how often we see multiple resources
    // for a frame before navigation finishes.
    ongoing_navigation_resources_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(extra_request_info.frame_tree_node_id),
        std::forward_as_tuple(
            extra_request_info.url, extra_request_info.frame_tree_node_id,
            extra_request_info.was_cached, extra_request_info.raw_body_bytes,
            extra_request_info.original_network_content_length, nullptr,
            extra_request_info.resource_type));
    return;
  }

  page_bytes_ += extra_request_info.raw_body_bytes;
  if (!extra_request_info.was_cached)
    uncached_page_bytes_ += extra_request_info.raw_body_bytes;

  // Determine if the frame (or its ancestor) is an ad, if so attribute the
  // bytes to the highest ad ancestor.
  AdFrameData* ancestor_data = id_and_data->second;

  if (ancestor_data) {
    ancestor_data->frame_bytes += extra_request_info.raw_body_bytes;
    if (!extra_request_info.was_cached) {
      ancestor_data->frame_bytes_uncached += extra_request_info.raw_body_bytes;
    }
  }
}

void AdsPageLoadMetricsObserver::RecordHistograms() {
  if (page_bytes_ == 0)
    return;

  size_t total_ad_frame_bytes = 0;
  size_t uncached_ad_frame_bytes = 0;

  UMA_HISTOGRAM_COUNTS_1000(
      "PageLoad.Clients.Ads.Google.FrameCounts.AnyParentFrame.AdFrames",
      ad_frames_data_storage_.size());

  // Don't post UMA for pages that don't have ads.
  if (ad_frames_data_storage_.empty())
    return;

  for (const AdFrameData& ad_frame_data : ad_frames_data_storage_) {
    total_ad_frame_bytes += ad_frame_data.frame_bytes;
    uncached_ad_frame_bytes += ad_frame_data.frame_bytes_uncached;

    PAGE_BYTES_HISTOGRAM(
        "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Total",
        ad_frame_data.frame_bytes);
    PAGE_BYTES_HISTOGRAM(
        "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Network",
        ad_frame_data.frame_bytes_uncached);
    if (ad_frame_data.frame_bytes > 0) {
      UMA_HISTOGRAM_PERCENTAGE(
          "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.PercentNetwork",
          ad_frame_data.frame_bytes_uncached * 100 / ad_frame_data.frame_bytes);
    }
  }

  UMA_HISTOGRAM_COUNTS_1000(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.TotalFrames",
      top_level_subframe_count_);
  UMA_HISTOGRAM_COUNTS_1000(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.AdFrames",
      top_level_ad_frame_count_);

  DCHECK_LT(0, top_level_subframe_count_);  // Because ad frames isn't empty.
  UMA_HISTOGRAM_PERCENTAGE(
      "PageLoad.Clients.Ads.Google.FrameCounts.MainFrameParent.PercentAdFrames",
      top_level_ad_frame_count_ * 100 / top_level_subframe_count_);

  PAGE_BYTES_HISTOGRAM(
      "PageLoad.Clients.Ads.Google.Bytes.NonAdFrames.Aggregate.Total",
      page_bytes_ - total_ad_frame_bytes);

  PAGE_BYTES_HISTOGRAM("PageLoad.Clients.Ads.Google.Bytes.FullPage.Total",
                       page_bytes_);
  PAGE_BYTES_HISTOGRAM("PageLoad.Clients.Ads.Google.Bytes.FullPage.Network",
                       uncached_page_bytes_);
  if (page_bytes_) {
    UMA_HISTOGRAM_PERCENTAGE(
        "PageLoad.Clients.Ads.Google.Bytes.FullPage.Total.PercentAds",
        total_ad_frame_bytes * 100 / page_bytes_);
  }
  if (uncached_page_bytes_ > 0) {
    UMA_HISTOGRAM_PERCENTAGE(
        "PageLoad.Clients.Ads.Google.Bytes.FullPage.Network.PercentAds",
        uncached_ad_frame_bytes * 100 / uncached_page_bytes_);
  }

  PAGE_BYTES_HISTOGRAM(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Total",
      total_ad_frame_bytes);
  PAGE_BYTES_HISTOGRAM(
      "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.Network",
      uncached_ad_frame_bytes);

  if (total_ad_frame_bytes) {
    UMA_HISTOGRAM_PERCENTAGE(
        "PageLoad.Clients.Ads.Google.Bytes.AdFrames.Aggregate.PercentNetwork",
        uncached_ad_frame_bytes * 100 / total_ad_frame_bytes);
  }
}

void AdsPageLoadMetricsObserver::ProcessOngoingNavigationResource(
    FrameTreeNodeId frame_tree_node_id) {
  const auto& frame_id_and_request =
      ongoing_navigation_resources_.find(frame_tree_node_id);
  if (frame_id_and_request == ongoing_navigation_resources_.end())
    return;

  ProcessLoadedResource(frame_id_and_request->second);
  ongoing_navigation_resources_.erase(frame_id_and_request);
}
