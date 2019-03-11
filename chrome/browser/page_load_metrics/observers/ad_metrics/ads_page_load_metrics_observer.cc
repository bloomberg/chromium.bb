// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ad_metrics/ads_page_load_metrics_observer.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/download/download_stats.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace {

#define ADS_HISTOGRAM(suffix, hist_macro, visibility, value)        \
  switch (visibility) {                                             \
    case FrameData::kNonVisible:                                    \
      hist_macro("PageLoad.Clients.Ads.NonVisible." suffix, value); \
      break;                                                        \
    case FrameData::kVisible:                                       \
      hist_macro("PageLoad.Clients.Ads.Visible." suffix, value);    \
      break;                                                        \
    case FrameData::kAnyVisibility:                                 \
      hist_macro("PageLoad.Clients.Ads." suffix, value);            \
      break;                                                        \
  }

#define RESOURCE_BYTES_HISTOGRAM(suffix, was_cached, value)                \
  if (was_cached) {                                                        \
    PAGE_BYTES_HISTOGRAM("Ads.ResourceUsage.Size.Cache." suffix, value);   \
  } else {                                                                 \
    PAGE_BYTES_HISTOGRAM("Ads.ResourceUsage.Size.Network." suffix, value); \
  }

// Finds the RenderFrameHost for the handle, possibly using the FrameTreeNode
// ID directly if the the handle has not been committed.
// NOTE: Unsafe with respect to security privileges.
content::RenderFrameHost* FindFrameMaybeUnsafe(
    content::NavigationHandle* handle) {
  return handle->HasCommitted()
             ? handle->GetRenderFrameHost()
             : handle->GetWebContents()->UnsafeFindFrameByFrameTreeNodeId(
                   handle->GetFrameTreeNodeId());
}

void RecordSingleFeatureUsage(content::RenderFrameHost* rfh,
                              blink::mojom::WebFeature web_feature) {
  page_load_metrics::mojom::PageLoadFeatures page_load_features(
      {web_feature}, {} /* css_properties */, {} /* animated_css_properties */);
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      rfh, page_load_features);
}

using ResourceMimeType = AdsPageLoadMetricsObserver::ResourceMimeType;

}  // namespace

// static
std::unique_ptr<AdsPageLoadMetricsObserver>
AdsPageLoadMetricsObserver::CreateIfNeeded() {
  if (!base::FeatureList::IsEnabled(subresource_filter::kAdTagging))
    return nullptr;
  return std::make_unique<AdsPageLoadMetricsObserver>();
}

// static
bool AdsPageLoadMetricsObserver::IsSubframeSameOriginToMainFrame(
    content::RenderFrameHost* sub_host,
    bool use_parent_origin) {
  DCHECK(sub_host);
  content::RenderFrameHost* main_host =
      content::WebContents::FromRenderFrameHost(sub_host)->GetMainFrame();
  if (use_parent_origin)
    sub_host = sub_host->GetParent();
  url::Origin subframe_origin = sub_host->GetLastCommittedOrigin();
  url::Origin mainframe_origin = main_host->GetLastCommittedOrigin();
  return subframe_origin.IsSameOriginWith(mainframe_origin);
}

AdsPageLoadMetricsObserver::AdsPageLoadMetricsObserver()
    : subresource_observer_(this) {}

AdsPageLoadMetricsObserver::~AdsPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AdsPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  auto* observer_manager =
      subresource_filter::SubresourceFilterObserverManager::FromWebContents(
          navigation_handle->GetWebContents());
  // |observer_manager| isn't constructed if the feature for subresource
  // filtering isn't enabled.
  if (observer_manager)
    subresource_observer_.Add(observer_manager);
  main_frame_data_ =
      std::make_unique<FrameData>(navigation_handle->GetFrameTreeNodeId());
  aggregate_frame_data_ =
      std::make_unique<FrameData>(navigation_handle->GetFrameTreeNodeId());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AdsPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  DCHECK(ad_frames_data_.empty());

  committed_ = true;
  aggregate_frame_data_->UpdateForNavigation(
      navigation_handle->GetRenderFrameHost(), true /* frame_navigated */);
  main_frame_data_->UpdateForNavigation(navigation_handle->GetRenderFrameHost(),
                                        true /* frame_navigated */);

  // The main frame is never considered an ad.
  ad_frames_data_[navigation_handle->GetFrameTreeNodeId()] = nullptr;
  ProcessOngoingNavigationResource(navigation_handle->GetFrameTreeNodeId());
  return CONTINUE_OBSERVING;
}

// Given an ad being triggered for a frame or navigation, get its FrameData
// and record it into the appropriate data structures.
void AdsPageLoadMetricsObserver::RecordAdFrameData(
    FrameTreeNodeId ad_id,
    bool is_adframe,
    content::RenderFrameHost* ad_host,
    bool frame_navigated) {
  // If an existing subframe is navigating and it was an ad previously that
  // hasn't navigated yet, then we need to update it.
  const auto& id_and_data = ad_frames_data_.find(ad_id);
  FrameData* previous_data = nullptr;
  if (id_and_data != ad_frames_data_.end() && id_and_data->second) {
    DCHECK(frame_navigated);
    if (id_and_data->second->frame_navigated()) {
      ProcessOngoingNavigationResource(ad_id);
      return;
    }
    previous_data = id_and_data->second;
  }

  // Determine who the parent frame's ad ancestor is.  If we don't know who it
  // is, return, such as with a frame from a previous navigation.
  content::RenderFrameHost* parent_frame_host =
      ad_host ? ad_host->GetParent() : nullptr;
  const auto& parent_id_and_data =
      parent_frame_host
          ? ad_frames_data_.find(parent_frame_host->GetFrameTreeNodeId())
          : ad_frames_data_.end();
  bool parent_exists = parent_id_and_data != ad_frames_data_.end();
  if (!parent_exists)
    return;

  // This frame is not nested within an ad frame but is itself an ad.
  FrameData* ad_data = parent_id_and_data->second;

  // If data existed already, update it and exit, otherwise, add it.
  if (!ad_data && is_adframe) {
    if (previous_data) {
      previous_data->UpdateForNavigation(ad_host, frame_navigated);
      return;
    }

    // If there is not existing data for this frame then create it.
    ad_frames_data_storage_.emplace_back(ad_id);
    ad_data = &ad_frames_data_storage_.back();
    ad_data->UpdateForNavigation(ad_host, frame_navigated);
  }

  // If there was previous data, then we don't want to overwrite this frame.
  if (!previous_data)
    ad_frames_data_[ad_id] = ad_data;
}

void AdsPageLoadMetricsObserver::ReadyToCommitNextNavigation(
    content::NavigationHandle* navigation_handle) {
  // When the renderer receives a CommitNavigation message for the main frame,
  // all subframes detach and become display : none. Since this is not user
  // visible, and not reflective of the frames state during the page lifetime,
  // ignore any such messages when a navigation is about to commit.
  if (!navigation_handle->IsInMainFrame())
    return;
  process_display_state_updates_ = false;
}

// Determine if the frame is part of an existing ad, the root of a new ad, or a
// non-ad frame. Once a frame is labeled as an ad, it is always considered an
// ad, even if it navigates to a non-ad page. This function labels all of a
// page's frames, even those that fail to commit.
void AdsPageLoadMetricsObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  FrameTreeNodeId frame_tree_node_id = navigation_handle->GetFrameTreeNodeId();
  bool is_adframe = DetectAds(navigation_handle);

  // NOTE: Frame look-up only used for determining cross-origin status, not
  // granting security permissions.
  content::RenderFrameHost* ad_host = FindFrameMaybeUnsafe(navigation_handle);

  if (navigation_handle->IsDownload()) {
    bool has_sandbox = ad_host->IsSandboxed(blink::WebSandboxFlags::kDownloads);
    bool has_gesture = navigation_handle->HasUserGesture();

    std::vector<blink::mojom::WebFeature> web_features;
    if (is_adframe) {
      // Note: Here it covers download due to navigations to non-web-renderable
      // content. These two features can also be logged from blink for download
      // originated from clicking on <a download> link that results in direct
      // download.
      blink::mojom::WebFeature web_feature =
          has_gesture
              ? blink::mojom::WebFeature::kDownloadInAdFrameWithUserGesture
              : blink::mojom::WebFeature::kDownloadInAdFrameWithoutUserGesture;
      RecordSingleFeatureUsage(ad_host, web_feature);
    }
    if (has_sandbox) {
      blink::mojom::WebFeature web_feature =
          has_gesture ? blink::mojom::WebFeature::
                            kNavigationDownloadInSandboxWithUserGesture
                      : blink::mojom::WebFeature::
                            kNavigationDownloadInSandboxWithoutUserGesture;
      RecordSingleFeatureUsage(ad_host, web_feature);
    }

    blink::DownloadStats::SubframeDownloadFlags flags;
    flags.has_sandbox = has_sandbox;
    flags.is_cross_origin =
        !IsSubframeSameOriginToMainFrame(ad_host, /*use_parent_origin=*/false);
    flags.is_ad_frame = is_adframe;
    flags.has_gesture = has_gesture;
    blink::DownloadStats::RecordSubframeDownloadFlags(
        flags,
        ukm::GetSourceIdForWebContentsDocument(
            navigation_handle->GetWebContents()),
        ukm::UkmRecorder::Get());
  }

  RecordAdFrameData(frame_tree_node_id, is_adframe, ad_host,
                    /*frame_navigated=*/true);
  ProcessOngoingNavigationResource(frame_tree_node_id);
}

void AdsPageLoadMetricsObserver::FrameReceivedFirstUserActivation(
    content::RenderFrameHost* render_frame_host) {
  const auto& id_and_data =
      ad_frames_data_.find(render_frame_host->GetFrameTreeNodeId());
  if (id_and_data == ad_frames_data_.end())
    return;
  FrameData* ancestor_data = id_and_data->second;
  if (ancestor_data)
    ancestor_data->set_received_user_activation();
}

void AdsPageLoadMetricsObserver::OnDidInternalNavigationAbort(
    content::NavigationHandle* navigation_handle) {
  // Main frame navigation
  if (navigation_handle->IsDownload()) {
    content::RenderFrameHost* rfh = FindFrameMaybeUnsafe(navigation_handle);
    bool has_sandbox = rfh->IsSandboxed(blink::WebSandboxFlags::kDownloads);
    bool has_gesture = navigation_handle->HasUserGesture();
    if (has_sandbox) {
      blink::mojom::WebFeature web_feature =
          has_gesture ? blink::mojom::WebFeature::
                            kNavigationDownloadInSandboxWithUserGesture
                      : blink::mojom::WebFeature::
                            kNavigationDownloadInSandboxWithoutUserGesture;
      RecordSingleFeatureUsage(rfh, web_feature);
    }

    blink::DownloadStats::MainFrameDownloadFlags flags;
    flags.has_sandbox = has_sandbox;
    flags.has_gesture = has_gesture;
    blink::DownloadStats::RecordMainFrameDownloadFlags(
        flags,
        ukm::GetSourceIdForWebContentsDocument(
            navigation_handle->GetWebContents()),
        ukm::UkmRecorder::Get());
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AdsPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  // The browser may come back, but there is no guarantee. To be safe, record
  // what we have now and ignore future changes to this navigation.
  if (extra_info.did_commit) {
    if (timing.response_start)
      time_commit_ = timing.navigation_start + *timing.response_start;
    RecordHistograms(extra_info.source_id);
  }

  return STOP_OBSERVING;
}

void AdsPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (info.did_commit && timing.response_start)
    time_commit_ = timing.navigation_start + *timing.response_start;
  RecordHistograms(info.source_id);
}

void AdsPageLoadMetricsObserver::OnResourceDataUseObserved(
    FrameTreeNodeId frame_tree_node_id,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  for (auto const& resource : resources)
    UpdateResource(frame_tree_node_id, resource);
}

void AdsPageLoadMetricsObserver::OnSubframeNavigationEvaluated(
    content::NavigationHandle* navigation_handle,
    subresource_filter::LoadPolicy load_policy,
    bool is_ad_subframe) {
  // We don't track DISALLOW frames because their resources won't be loaded
  // and therefore would provide bad histogram data. Note that WOULD_DISALLOW
  // is only seen in dry runs.
  if (is_ad_subframe &&
      load_policy != subresource_filter::LoadPolicy::DISALLOW) {
    unfinished_subresource_ad_frames_.insert(
        navigation_handle->GetFrameTreeNodeId());
  }
}

void AdsPageLoadMetricsObserver::OnPageInteractive(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (timing.interactive_timing->interactive) {
    time_interactive_ =
        timing.navigation_start + *timing.interactive_timing->interactive;
    page_ad_bytes_at_interactive_ = aggregate_frame_data_->ad_network_bytes();
  }
}

void AdsPageLoadMetricsObserver::FrameDisplayStateChanged(
    content::RenderFrameHost* render_frame_host,
    bool is_display_none) {
  if (!process_display_state_updates_)
    return;
  const auto& id_and_data =
      ad_frames_data_.find(render_frame_host->GetFrameTreeNodeId());
  if (id_and_data == ad_frames_data_.end())
    return;
  FrameData* ancestor_data = id_and_data->second;
  // If the frame whose display state has changed is the root of the ad ancestry
  // chain, then update it. The display property is propagated to all child
  // frames.
  if (ancestor_data && render_frame_host->GetFrameTreeNodeId() ==
                           ancestor_data->frame_tree_node_id()) {
    ancestor_data->SetDisplayState(is_display_none);
  }
}

void AdsPageLoadMetricsObserver::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  const auto& id_and_data =
      ad_frames_data_.find(render_frame_host->GetFrameTreeNodeId());
  if (id_and_data == ad_frames_data_.end())
    return;
  FrameData* ancestor_data = id_and_data->second;
  // If the frame whose size has changed is the root of the ad ancestry chain,
  // then update it
  if (ancestor_data && render_frame_host->GetFrameTreeNodeId() ==
                           ancestor_data->frame_tree_node_id()) {
    ancestor_data->SetFrameSize(frame_size);
  }
}

void AdsPageLoadMetricsObserver::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    content::RenderFrameHost* render_frame_host) {
  aggregate_frame_data_->set_media_status(FrameData::MediaStatus::kPlayed);
  if (render_frame_host == GetDelegate()->GetWebContents()->GetMainFrame())
    main_frame_data_->set_media_status(FrameData::MediaStatus::kPlayed);

  const auto& id_and_data =
      ad_frames_data_.find(render_frame_host->GetFrameTreeNodeId());
  if (id_and_data == ad_frames_data_.end())
    return;
  FrameData* ancestor_data = id_and_data->second;
  if (ancestor_data)
    ancestor_data->set_media_status(FrameData::MediaStatus::kPlayed);
}

void AdsPageLoadMetricsObserver::OnAdSubframeDetected(
    content::RenderFrameHost* render_frame_host) {
  FrameTreeNodeId frame_tree_node_id = render_frame_host->GetFrameTreeNodeId();
  RecordAdFrameData(frame_tree_node_id, true /* is_adframe */,
                    render_frame_host,
                    /*frame_navigated=*/false);
}

void AdsPageLoadMetricsObserver::OnSubresourceFilterGoingAway() {
  subresource_observer_.RemoveAll();
}

bool AdsPageLoadMetricsObserver::DetectSubresourceFilterAd(
    FrameTreeNodeId frame_tree_node_id) {
  return unfinished_subresource_ad_frames_.erase(frame_tree_node_id);
}

bool AdsPageLoadMetricsObserver::DetectAds(
    content::NavigationHandle* navigation_handle) {
  return DetectSubresourceFilterAd(navigation_handle->GetFrameTreeNodeId());
}

void AdsPageLoadMetricsObserver::ProcessResourceForFrame(
    FrameTreeNodeId frame_tree_node_id,
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  const auto& id_and_data = ad_frames_data_.find(frame_tree_node_id);
  if (id_and_data == ad_frames_data_.end()) {
    if (resource->is_primary_frame_resource) {
      // Only hold onto primary resources if their load has finished.
      if (!resource->is_complete)
        return;

      // This resource request is the primary resource load for a frame that
      // hasn't yet finished navigating. Hang onto the request info and replay
      // it once the frame finishes navigating.
      ongoing_navigation_resources_.emplace(
          std::piecewise_construct, std::forward_as_tuple(frame_tree_node_id),
          std::forward_as_tuple(resource.Clone()));
    } else {
      // This is unexpected, it could be:
      // 1. a resource from a previous navigation that started its resource
      //    load after this page started navigation.
      // 2. possibly a resource from a document.written frame whose frame
      //    failure message has yet to arrive. (uncertain of this)
    }
    return;
  }

  aggregate_frame_data_->ProcessResourceLoadInFrame(resource);
  if (resource->is_main_frame_resource)
    main_frame_data_->ProcessResourceLoadInFrame(resource);

  // Determine if the frame (or its ancestor) is an ad, if so attribute the
  // bytes to the highest ad ancestor.
  FrameData* ancestor_data = id_and_data->second;
  if (!ancestor_data)
    return;
  ancestor_data->ProcessResourceLoadInFrame(resource);

  if (ancestor_data->size_intervention_status() ==
      FrameData::FrameSizeInterventionStatus::kTriggered) {
    RecordSingleFeatureUsage(
        GetDelegate()->GetWebContents()->GetMainFrame(),
        blink::mojom::WebFeature::kAdFrameSizeIntervention);
  }
}

void AdsPageLoadMetricsObserver::AdjustAdBytesForFrame(
    FrameTreeNodeId frame_tree_node_id,
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource,
    int64_t unaccounted_ad_bytes) {
  const auto& id_and_data = ad_frames_data_.find(frame_tree_node_id);
  FrameData* ancestor_data = id_and_data->second;
  if (!ancestor_data)
    return;
  ancestor_data->AdjustAdBytes(unaccounted_ad_bytes,
                               FrameData::GetResourceMimeType(resource));
}

void AdsPageLoadMetricsObserver::UpdateResource(
    FrameTreeNodeId frame_tree_node_id,
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  ProcessResourceForFrame(frame_tree_node_id, resource);
  auto it = page_resources_.find(resource->request_id);

  if (resource->reported_as_ad_resource) {
    // If the resource had already started loading, and is now labeled as an ad,
    // but was not before, we need to account for all the previously received
    // bytes.
    bool is_new_ad =
        (it != page_resources_.end()) && !it->second->reported_as_ad_resource;
    int unaccounted_ad_bytes =
        is_new_ad ? resource->received_data_length - resource->delta_bytes : 0;
    if (unaccounted_ad_bytes)
      AdjustAdBytesForFrame(frame_tree_node_id, resource, unaccounted_ad_bytes);
  }

  // Update resource map.
  if (resource->is_complete) {
    RecordResourceHistograms(resource);
    if (it != page_resources_.end())
      page_resources_.erase(it);
  } else {
    // Must clone resource so it will be accessible when the observer is
    // destroyed.
    if (it != page_resources_.end()) {
      it->second = resource->Clone();
    } else {
      page_resources_.emplace(std::piecewise_construct,
                              std::forward_as_tuple(resource->request_id),
                              std::forward_as_tuple(resource->Clone()));
    }
  }
}

void AdsPageLoadMetricsObserver::RecordResourceMimeHistograms(
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  int64_t data_length = resource->was_fetched_via_cache
                            ? resource->encoded_body_length
                            : resource->received_data_length;
  ResourceMimeType mime_type = FrameData::GetResourceMimeType(resource);
  if (mime_type == ResourceMimeType::kImage) {
    RESOURCE_BYTES_HISTOGRAM("Mime.Image", resource->was_fetched_via_cache,
                             data_length);
  } else if (mime_type == ResourceMimeType::kJavascript) {
    RESOURCE_BYTES_HISTOGRAM("Mime.JS", resource->was_fetched_via_cache,
                             data_length);
  } else if (mime_type == ResourceMimeType::kVideo) {
    RESOURCE_BYTES_HISTOGRAM("Mime.Video", resource->was_fetched_via_cache,
                             data_length);
  } else if (mime_type == ResourceMimeType::kCss) {
    RESOURCE_BYTES_HISTOGRAM("Mime.CSS", resource->was_fetched_via_cache,
                             data_length);
  } else if (mime_type == ResourceMimeType::kHtml) {
    RESOURCE_BYTES_HISTOGRAM("Mime.HTML", resource->was_fetched_via_cache,
                             data_length);
  } else if (mime_type == ResourceMimeType::kOther) {
    RESOURCE_BYTES_HISTOGRAM("Mime.Other", resource->was_fetched_via_cache,
                             data_length);
  }
}

void AdsPageLoadMetricsObserver::RecordResourceHistograms(
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  int64_t data_length = resource->was_fetched_via_cache
                            ? resource->encoded_body_length
                            : resource->received_data_length;
  if (resource->is_main_frame_resource && resource->reported_as_ad_resource) {
    RESOURCE_BYTES_HISTOGRAM("Mainframe.AdResource",
                             resource->was_fetched_via_cache, data_length);
  } else if (resource->is_main_frame_resource) {
    RESOURCE_BYTES_HISTOGRAM("Mainframe.VanillaResource",
                             resource->was_fetched_via_cache, data_length);
  } else if (resource->reported_as_ad_resource) {
    RESOURCE_BYTES_HISTOGRAM("Subframe.AdResource",
                             resource->was_fetched_via_cache, data_length);
  } else {
    RESOURCE_BYTES_HISTOGRAM("Subframe.VanillaResource",
                             resource->was_fetched_via_cache, data_length);
  }

  // Only report sizes by mime type for ad resources.
  if (resource->reported_as_ad_resource)
    RecordResourceMimeHistograms(resource);
}

void AdsPageLoadMetricsObserver::RecordPageResourceTotalHistograms(
    ukm::SourceId source_id) {
  // Only records histograms on pages that have some ad bytes.
  if (aggregate_frame_data_->ad_bytes() == 0)
    return;
  PAGE_BYTES_HISTOGRAM("PageLoad.Clients.Ads.Resources.Bytes.Ads",
                       aggregate_frame_data_->ad_bytes());
  size_t unfinished_bytes = 0;
  for (auto const& kv : page_resources_)
    unfinished_bytes += kv.second->received_data_length;
  PAGE_BYTES_HISTOGRAM("PageLoad.Clients.Ads.Resources.Bytes.Unfinished",
                       unfinished_bytes);

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::AdPageLoad builder(source_id);
  builder.SetTotalBytes(aggregate_frame_data_->network_bytes() >> 10)
      .SetAdBytes(aggregate_frame_data_->ad_network_bytes() >> 10)
      .SetAdJavascriptBytes(
          aggregate_frame_data_->ad_javascript_network_bytes() >> 10)
      .SetAdVideoBytes(aggregate_frame_data_->ad_video_network_bytes() >> 10);
  base::Time current_time = base::Time::Now();
  if (!time_commit_.is_null()) {
    int time_since_commit = (current_time - time_commit_).InMicroseconds();
    if (time_since_commit > 0) {
      int ad_kbps_from_commit =
          (aggregate_frame_data_->ad_network_bytes() >> 10) * 1000 * 1000 /
          time_since_commit;
      builder.SetAdBytesPerSecond(ad_kbps_from_commit);
    }
  }
  if (!time_interactive_.is_null()) {
    int time_since_interactive =
        (current_time - time_interactive_).InMicroseconds();
    int64_t bytes_since_interactive =
        aggregate_frame_data_->ad_network_bytes() -
        page_ad_bytes_at_interactive_;
    if (time_since_interactive > 0) {
      int ad_kbps_since_interactive = (bytes_since_interactive >> 10) * 1000 *
                                      1000 / time_since_interactive;
      builder.SetAdBytesPerSecondAfterInteractive(ad_kbps_since_interactive);
    }
  }
  builder.Record(ukm_recorder->Get());
}

void AdsPageLoadMetricsObserver::RecordHistograms(ukm::SourceId source_id) {
  RecordHistogramsForAdTagging(FrameData::FrameVisibility::kNonVisible);
  RecordHistogramsForAdTagging(FrameData::FrameVisibility::kVisible);
  RecordHistogramsForAdTagging(FrameData::FrameVisibility::kAnyVisibility);
  RecordPageResourceTotalHistograms(source_id);
  for (auto const& kv : page_resources_)
    RecordResourceHistograms(kv.second);
}

void AdsPageLoadMetricsObserver::RecordHistogramsForAdTagging(
    FrameData::FrameVisibility visibility) {
  if (aggregate_frame_data_->bytes() == 0)
    return;

  int non_zero_ad_frames = 0;
  size_t total_ad_frame_bytes = 0;
  size_t ad_frame_network_bytes = 0;

  for (const FrameData& ad_frame_data : ad_frames_data_storage_) {
    if (ad_frame_data.bytes() == 0)
      continue;

    if (visibility != FrameData::FrameVisibility::kAnyVisibility &&
        ad_frame_data.visibility() != visibility)
      continue;

    ADS_HISTOGRAM("FrameCounts.AdFrames.PerFrame.SqrtNumberOfPixels",
                  UMA_HISTOGRAM_COUNTS_10000, visibility,
                  std::sqrt(ad_frame_data.frame_size().GetArea()));
    ADS_HISTOGRAM("FrameCounts.AdFrames.PerFrame.SmallestDimension",
                  UMA_HISTOGRAM_COUNTS_10000, visibility,
                  std::min(ad_frame_data.frame_size().width(),
                           ad_frame_data.frame_size().height()));
    ADS_HISTOGRAM("FrameCounts.AdFrames.PerFrame.SizeIntervention",
                  UMA_HISTOGRAM_ENUMERATION, visibility,
                  ad_frame_data.size_intervention_status());

    if (ad_frame_data.size_intervention_status() ==
        FrameData::FrameSizeInterventionStatus::kTriggered) {
      ADS_HISTOGRAM(
          "FrameCounts.AdFrames.PerFrame.SizeIntervention.MediaStatus",
          UMA_HISTOGRAM_ENUMERATION, visibility, ad_frame_data.media_status());
    }

    non_zero_ad_frames += 1;
    total_ad_frame_bytes += ad_frame_data.bytes();
    ad_frame_network_bytes += ad_frame_data.network_bytes();

    ADS_HISTOGRAM("Bytes.AdFrames.PerFrame.Total", PAGE_BYTES_HISTOGRAM,
                  visibility, ad_frame_data.bytes());
    ADS_HISTOGRAM("Bytes.AdFrames.PerFrame.Network", PAGE_BYTES_HISTOGRAM,
                  visibility, ad_frame_data.network_bytes());
    ADS_HISTOGRAM("Bytes.AdFrames.PerFrame.SameOrigin", PAGE_BYTES_HISTOGRAM,
                  visibility, ad_frame_data.same_origin_bytes());
    ADS_HISTOGRAM("Bytes.AdFrames.PerFrame.PercentNetwork",
                  UMA_HISTOGRAM_PERCENTAGE, visibility,
                  ad_frame_data.network_bytes() * 100 / ad_frame_data.bytes());
    ADS_HISTOGRAM(
        "Bytes.AdFrames.PerFrame.PercentSameOrigin", UMA_HISTOGRAM_PERCENTAGE,
        visibility,
        ad_frame_data.same_origin_bytes() * 100 / ad_frame_data.bytes());
    ADS_HISTOGRAM("FrameCounts.AdFrames.PerFrame.OriginStatus",
                  UMA_HISTOGRAM_ENUMERATION, visibility,
                  ad_frame_data.origin_status());
    ADS_HISTOGRAM("FrameCounts.AdFrames.PerFrame.UserActivation",
                  UMA_HISTOGRAM_ENUMERATION, visibility,
                  ad_frame_data.user_activation_status());
  }

  // TODO(ericrobinson): Consider renaming this to match
  //   'FrameCounts.AdFrames.PerFrame.OriginStatus'.
  ADS_HISTOGRAM("FrameCounts.AnyParentFrame.AdFrames",
                UMA_HISTOGRAM_COUNTS_1000, visibility, non_zero_ad_frames);

  // Don't post UMA for pages that don't have ads.
  if (non_zero_ad_frames == 0)
    return;

  ADS_HISTOGRAM("Bytes.NonAdFrames.Aggregate.Total", PAGE_BYTES_HISTOGRAM,
                visibility,
                aggregate_frame_data_->bytes() - total_ad_frame_bytes);

  ADS_HISTOGRAM("Bytes.FullPage.Total", PAGE_BYTES_HISTOGRAM, visibility,
                aggregate_frame_data_->bytes());
  ADS_HISTOGRAM("Bytes.FullPage.Network", PAGE_BYTES_HISTOGRAM, visibility,
                aggregate_frame_data_->network_bytes());

  if (aggregate_frame_data_->bytes()) {
    ADS_HISTOGRAM("Bytes.FullPage.Total.PercentAds", UMA_HISTOGRAM_PERCENTAGE,
                  visibility,
                  total_ad_frame_bytes * 100 / aggregate_frame_data_->bytes());
  }
  if (aggregate_frame_data_->network_bytes()) {
    ADS_HISTOGRAM(
        "Bytes.FullPage.Network.PercentAds", UMA_HISTOGRAM_PERCENTAGE,
        visibility,
        ad_frame_network_bytes * 100 / aggregate_frame_data_->network_bytes());
  }

  ADS_HISTOGRAM("Bytes.AdFrames.Aggregate.Total", PAGE_BYTES_HISTOGRAM,
                visibility, total_ad_frame_bytes);
  ADS_HISTOGRAM("Bytes.AdFrames.Aggregate.Network", PAGE_BYTES_HISTOGRAM,
                visibility, ad_frame_network_bytes);

  if (total_ad_frame_bytes) {
    ADS_HISTOGRAM("Bytes.AdFrames.Aggregate.PercentNetwork",
                  UMA_HISTOGRAM_PERCENTAGE, visibility,
                  ad_frame_network_bytes * 100 / total_ad_frame_bytes);
  }

  // Only record same origin and main frame totals for the AnyVisibility suffix
  // as these numbers do not change for different visibility types.
  if (visibility != FrameData::FrameVisibility::kAnyVisibility)
    return;
  ADS_HISTOGRAM("Bytes.FullPage.SameOrigin", PAGE_BYTES_HISTOGRAM, visibility,
                aggregate_frame_data_->same_origin_bytes());
  if (aggregate_frame_data_->bytes()) {
    ADS_HISTOGRAM("Bytes.FullPage.PercentSameOrigin", UMA_HISTOGRAM_PERCENTAGE,
                  visibility,
                  aggregate_frame_data_->same_origin_bytes() * 100 /
                      aggregate_frame_data_->bytes());
  }
  ADS_HISTOGRAM("Bytes.MainFrame.Network", PAGE_BYTES_HISTOGRAM, visibility,
                main_frame_data_->network_bytes());
  ADS_HISTOGRAM("Bytes.MainFrame.Total", PAGE_BYTES_HISTOGRAM, visibility,
                main_frame_data_->bytes());
  ADS_HISTOGRAM("Bytes.MainFrame.Ads.Network", PAGE_BYTES_HISTOGRAM, visibility,
                main_frame_data_->ad_network_bytes());
  ADS_HISTOGRAM("Bytes.MainFrame.Ads.Total", PAGE_BYTES_HISTOGRAM, visibility,
                main_frame_data_->ad_bytes());
}

void AdsPageLoadMetricsObserver::ProcessOngoingNavigationResource(
    FrameTreeNodeId frame_tree_node_id) {
  const auto& frame_id_and_request =
      ongoing_navigation_resources_.find(frame_tree_node_id);
  if (frame_id_and_request == ongoing_navigation_resources_.end())
    return;

  ProcessResourceForFrame(frame_tree_node_id, frame_id_and_request->second);
  ongoing_navigation_resources_.erase(frame_id_and_request);
}
