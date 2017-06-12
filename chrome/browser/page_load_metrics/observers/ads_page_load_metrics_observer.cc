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

const base::Feature kAdsFeature{"AdsMetrics", base::FEATURE_ENABLED_BY_DEFAULT};

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

void RecordParentExistsForSubFrame(bool parent_exists) {
  UMA_HISTOGRAM_BOOLEAN("PageLoad.Clients.Ads.Google.ParentExistsForSubFrame",
                        parent_exists);
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
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  DCHECK(ad_frames_data_.empty());

  committed_ = true;

  // The main frame is never considered an ad.
  ad_frames_data_[navigation_handle->GetFrameTreeNodeId()] = nullptr;
  ProcessOngoingNavigationResource(navigation_handle->GetFrameTreeNodeId());
  return CONTINUE_OBSERVING;
}

void AdsPageLoadMetricsObserver::OnDidFinishSubFrameNavigation(
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
      return;
    }
    // This frame was previously not an ad, process it as usual. If it had
    // any child frames that were ads, those will still be recorded.
    UMA_HISTOGRAM_BOOLEAN(
        "PageLoad.Clients.Ads.Google.Navigations.NonAdFrameRenavigatedToAd",
        FrameIsAd(navigation_handle));
  }

  // Determine who the parent frame's ad ancestor is.
  const auto& parent_id_and_data =
      ad_frames_data_.find(parent_frame_host->GetFrameTreeNodeId());
  if (parent_id_and_data == ad_frames_data_.end()) {
    // We don't know who the parent for this frame is. One possibility is that
    // it's a frame from a previous navigation.
    RecordParentExistsForSubFrame(false /* parent_exists */);

    return;
  }
  RecordParentExistsForSubFrame(true /* parent_exists */);

  AdFrameData* ad_data = parent_id_and_data->second;

  if (!ad_data && FrameIsAd(navigation_handle)) {
    // This frame is not nested within an ad frame but is itself an ad.
    ad_frames_data_storage_.emplace_back(frame_tree_node_id);
    ad_data = &ad_frames_data_storage_.back();
  }

  ad_frames_data_[frame_tree_node_id] = ad_data;

  ProcessOngoingNavigationResource(frame_tree_node_id);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AdsPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing,
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
    const page_load_metrics::mojom::PageLoadTiming& timing,
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
    if (extra_request_info.resource_type == content::RESOURCE_TYPE_MAIN_FRAME ||
        extra_request_info.resource_type == content::RESOURCE_TYPE_SUB_FRAME) {
      // This resource request is the primary resource load for a frame that
      // hasn't yet finished navigating. Hang onto the request info and replay
      // it once the frame finishes navigating.
      ongoing_navigation_resources_.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(extra_request_info.frame_tree_node_id),
          std::forward_as_tuple(
              extra_request_info.url, extra_request_info.host_port_pair,
              extra_request_info.frame_tree_node_id,
              extra_request_info.was_cached, extra_request_info.raw_body_bytes,
              extra_request_info.original_network_content_length, nullptr,
              extra_request_info.resource_type, extra_request_info.net_error));
    } else {
      // This is unexpected, it could be:
      // 1. a resource from a previous navigation that started its resource
      //    load after this page started navigation.
      // 2. possibly a resource from a document.written frame whose frame
      //    failure message has yet to arrive. (uncertain of this)
    }
    if (committed_) {
      UMA_HISTOGRAM_ENUMERATION(
          "PageLoad.Clients.Ads.Google.ResourceTypeWhenNoFrameFound",
          extra_request_info.resource_type, content::RESOURCE_TYPE_LAST_TYPE);
    }

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

  int non_zero_ad_frames = 0;
  size_t total_ad_frame_bytes = 0;
  size_t uncached_ad_frame_bytes = 0;

  for (const AdFrameData& ad_frame_data : ad_frames_data_storage_) {
    if (ad_frame_data.frame_bytes == 0)
      continue;

    non_zero_ad_frames += 1;
    total_ad_frame_bytes += ad_frame_data.frame_bytes;
    uncached_ad_frame_bytes += ad_frame_data.frame_bytes_uncached;

    PAGE_BYTES_HISTOGRAM(
        "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Total",
        ad_frame_data.frame_bytes);
    PAGE_BYTES_HISTOGRAM(
        "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.Network",
        ad_frame_data.frame_bytes_uncached);
    UMA_HISTOGRAM_PERCENTAGE(
        "PageLoad.Clients.Ads.Google.Bytes.AdFrames.PerFrame.PercentNetwork",
        ad_frame_data.frame_bytes_uncached * 100 / ad_frame_data.frame_bytes);
  }

  UMA_HISTOGRAM_COUNTS_1000(
      "PageLoad.Clients.Ads.Google.FrameCounts.AnyParentFrame.AdFrames",
      non_zero_ad_frames);

  // Don't post UMA for pages that don't have ads.
  if (non_zero_ad_frames == 0)
    return;

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
