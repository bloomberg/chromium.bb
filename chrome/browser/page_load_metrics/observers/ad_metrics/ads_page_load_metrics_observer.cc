// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ad_metrics/ads_page_load_metrics_observer.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_blocklist.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_helper.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_service.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_service_factory.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/browser/page_load_metrics/resource_tracker.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/common/chrome_features.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_errors.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/base/page_transition_types.h"
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

using ResourceMimeType = AdsPageLoadMetricsObserver::ResourceMimeType;

}  // namespace

// static
std::unique_ptr<AdsPageLoadMetricsObserver>
AdsPageLoadMetricsObserver::CreateIfNeeded(content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(subresource_filter::kAdTagging) ||
      !ChromeSubresourceFilterClient::FromWebContents(web_contents))
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

AdsPageLoadMetricsObserver::AggregateFrameInfo::AggregateFrameInfo()
    : bytes(0), network_bytes(0), num_frames(0) {}

AdsPageLoadMetricsObserver::AdsPageLoadMetricsObserver(
    base::TickClock* clock,
    HeavyAdBlocklist* blocklist)
    : subresource_observer_(this),
      clock_(clock ? clock : base::DefaultTickClock::GetInstance()),
      heavy_ad_blocklist_(blocklist),
      heavy_ad_blocklist_enabled_(
          base::FeatureList::IsEnabled(features::kHeavyAdBlocklist)) {}

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
  ad_frames_data_[navigation_handle->GetFrameTreeNodeId()] =
      ad_frames_data_storage_.end();
  ProcessOngoingNavigationResource(navigation_handle->GetRenderFrameHost());
  return CONTINUE_OBSERVING;
}

void AdsPageLoadMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!subframe_rfh)
    return;

  FrameData* ancestor_data = FindFrameData(subframe_rfh->GetFrameTreeNodeId());

  // Only update the frame with the root frames timing updates.
  if (ancestor_data &&
      ancestor_data->frame_tree_node_id() == subframe_rfh->GetFrameTreeNodeId())
    ancestor_data->set_timing(timing.Clone());
}

void AdsPageLoadMetricsObserver::MaybeTriggerHeavyAdIntervention(
    content::RenderFrameHost* render_frame_host,
    FrameData* frame_data) {
  DCHECK(render_frame_host);
  if (!frame_data->MaybeTriggerHeavyAdIntervention())
    return;

  // Check to see if we are allowed to activate on this host.
  if (IsBlocklisted())
    return;

  // Find the RenderFrameHost associated with this frame data. It is possible
  // that this frame no longer exists. We do not care if the frame has
  // moved to a new process because once the frame has been tagged as an ad, it
  // is always considered an ad.
  while (render_frame_host && render_frame_host->GetFrameTreeNodeId() !=
                                  frame_data->frame_tree_node_id()) {
    render_frame_host = render_frame_host->GetParent();
  }
  if (!render_frame_host)
    return;

  // Ensure that this RenderFrameHost is a subframe.
  DCHECK(render_frame_host->GetParent());

  GetDelegate().GetWebContents()->GetController().LoadErrorPage(
      render_frame_host, render_frame_host->GetLastCommittedURL(),
      heavy_ads::PrepareHeavyAdPage(), net::ERR_BLOCKED_BY_CLIENT);

  ADS_HISTOGRAM("HeavyAds.InterventionType2", UMA_HISTOGRAM_ENUMERATION,
                FrameData::FrameVisibility::kAnyVisibility,
                frame_data->heavy_ad_status());
  ADS_HISTOGRAM("HeavyAds.InterventionType2", UMA_HISTOGRAM_ENUMERATION,
                frame_data->visibility(), frame_data->heavy_ad_status());

  // Report intervention to the blocklist.
  if (auto* blocklist = GetHeavyAdBlocklist()) {
    blocklist->AddEntry(
        GetDelegate().GetWebContents()->GetLastCommittedURL().host(),
        true /* opt_out */,
        static_cast<int>(HeavyAdBlocklistType::kHeavyAdOnlyType));
  }
}

void AdsPageLoadMetricsObserver::OnCpuTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::CpuTiming& timing) {
  // We should never trigger if the timing is null, no data should be sent.
  DCHECK(!timing.task_time.is_zero());

  // If the page is backgrounded, don't update CPU times.
  if (!GetDelegate().GetVisibilityTracker().currently_in_foreground())
    return;

  // Get the current time, considered to be when this update occurred.
  base::TimeTicks current_time = clock_->NowTicks();

  FrameData::InteractiveStatus interactive_status =
      time_interactive_.is_null()
          ? FrameData::InteractiveStatus::kPreInteractive
          : FrameData::InteractiveStatus::kPostInteractive;
  aggregate_frame_data_->UpdateCpuUsage(current_time, timing.task_time,
                                        interactive_status);

  FrameData* ancestor_data = FindFrameData(subframe_rfh->GetFrameTreeNodeId());
  if (ancestor_data) {
    ancestor_data->UpdateCpuUsage(current_time, timing.task_time,
                                  interactive_status);
    MaybeTriggerHeavyAdIntervention(subframe_rfh, ancestor_data);
  }
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
  if (id_and_data != ad_frames_data_.end() &&
      id_and_data->second != ad_frames_data_storage_.end()) {
    DCHECK(frame_navigated);
    if ((*id_and_data->second).frame_navigated()) {
      ProcessOngoingNavigationResource(ad_host);
      return;
    }
    previous_data = &*id_and_data->second;
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

  auto ad_data_iterator = parent_id_and_data->second;

  FrameData* ad_data = nullptr;
  if (parent_id_and_data->second != ad_frames_data_storage_.end())
    ad_data = &*ad_data_iterator;

  if (!ad_data && is_adframe) {
    if (previous_data) {
      previous_data->UpdateForNavigation(ad_host, frame_navigated);
      return;
    }
    ad_frames_data_storage_.emplace_back(ad_id);
    ad_data_iterator = --ad_frames_data_storage_.end();
    ad_data = &*ad_data_iterator;
    ad_data->UpdateForNavigation(ad_host, frame_navigated);
  }

  // Maybe update frame depth based on the new ad frames distance to the ad
  // root.
  if (ad_data)
    ad_data->MaybeUpdateFrameDepth(ad_host);

  // If there was previous data, then we don't want to overwrite this frame.
  if (!previous_data)
    ad_frames_data_[ad_id] = ad_data_iterator;
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
    content::NavigationHandle* navigation_handle) {
  // If the AdsPageLoadMetricsObserver is created, this does not return nullptr.
  auto* client = ChromeSubresourceFilterClient::FromWebContents(
      navigation_handle->GetWebContents());
  // AdsPageLoadMetricsObserver is not created unless there is a
  // ChromeSubresourceFilterClient
  DCHECK(client);
  FrameTreeNodeId frame_tree_node_id = navigation_handle->GetFrameTreeNodeId();

  // NOTE: Frame look-up only used for determining cross-origin status, not
  // granting security permissions.
  content::RenderFrameHost* frame_host =
      FindFrameMaybeUnsafe(navigation_handle);

  bool is_adframe = client->GetThrottleManager()->IsFrameTaggedAsAd(frame_host);

  RecordAdFrameData(frame_tree_node_id, is_adframe, frame_host,
                    /*frame_navigated=*/true);
  ProcessOngoingNavigationResource(frame_host);
}

void AdsPageLoadMetricsObserver::FrameReceivedFirstUserActivation(
    content::RenderFrameHost* render_frame_host) {
  FrameData* ancestor_data =
      FindFrameData(render_frame_host->GetFrameTreeNodeId());
  if (ancestor_data) {
    ancestor_data->SetReceivedUserActivation(
        GetDelegate().GetVisibilityTracker().GetForegroundDuration());
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AdsPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // The browser may come back, but there is no guarantee. To be safe, record
  // what we have now and ignore future changes to this navigation.
  if (GetDelegate().DidCommit()) {
    if (timing.response_start) {
      time_commit_ =
          GetDelegate().GetNavigationStart() + *timing.response_start;
    }
    RecordHistograms(GetDelegate().GetSourceId());
  }

  return STOP_OBSERVING;
}

void AdsPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().DidCommit() && timing.response_start)
    time_commit_ = GetDelegate().GetNavigationStart() + *timing.response_start;
  RecordHistograms(GetDelegate().GetSourceId());
}

void AdsPageLoadMetricsObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  for (auto const& resource : resources) {
    ProcessResourceForPage(rfh->GetProcess()->GetID(), resource);
    ProcessResourceForFrame(rfh, resource);
  }
}

void AdsPageLoadMetricsObserver::OnPageInteractive(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (timing.interactive_timing->interactive) {
    time_interactive_ = GetDelegate().GetNavigationStart() +
                        *timing.interactive_timing->interactive;
    pre_interactive_duration_ =
        GetDelegate().GetVisibilityTracker().GetForegroundDuration();
    page_ad_bytes_at_interactive_ = aggregate_frame_data_->ad_network_bytes();
  }
}

void AdsPageLoadMetricsObserver::FrameDisplayStateChanged(
    content::RenderFrameHost* render_frame_host,
    bool is_display_none) {
  if (!process_display_state_updates_)
    return;
  FrameData* ancestor_data =
      FindFrameData(render_frame_host->GetFrameTreeNodeId());
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
  FrameData* ancestor_data =
      FindFrameData(render_frame_host->GetFrameTreeNodeId());
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
  if (render_frame_host == GetDelegate().GetWebContents()->GetMainFrame())
    main_frame_data_->set_media_status(FrameData::MediaStatus::kPlayed);

  FrameData* ancestor_data =
      FindFrameData(render_frame_host->GetFrameTreeNodeId());
  if (ancestor_data)
    ancestor_data->set_media_status(FrameData::MediaStatus::kPlayed);
}

void AdsPageLoadMetricsObserver::OnFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host)
    return;

  const auto& id_and_data =
      ad_frames_data_.find(render_frame_host->GetFrameTreeNodeId());
  if (id_and_data == ad_frames_data_.end())
    return;

  FrameData* ancestor_data = nullptr;
  if (id_and_data->second != ad_frames_data_storage_.end())
    ancestor_data = &*id_and_data->second;

  DCHECK_EQ(id_and_data->second == ad_frames_data_storage_.end(),
            !ancestor_data);

  // If the root ad frame has been deleted, flush histograms for the frame and
  // remove it from storage. All child frames should be deleted by this point.
  if (ancestor_data && ancestor_data->frame_tree_node_id() ==
                           render_frame_host->GetFrameTreeNodeId()) {
    RecordPerFrameHistogramsForAdTagging(*ancestor_data);
    RecordPerFrameHistogramsForCpuUsage(*ancestor_data);
    ancestor_data->RecordAdFrameLoadUkmEvent(GetDelegate().GetSourceId());
    DCHECK(id_and_data->second != ad_frames_data_storage_.end());
    ad_frames_data_storage_.erase(id_and_data->second);
  }

  // Delete this frame's entry from the map now that the store is deleted.
  ad_frames_data_.erase(id_and_data);
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

int AdsPageLoadMetricsObserver::GetUnaccountedAdBytes(
    int process_id,
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) const {
  if (!resource->reported_as_ad_resource)
    return 0;
  content::GlobalRequestID global_request_id(process_id, resource->request_id);

  // Resource just started loading.
  if (!GetDelegate().GetResourceTracker().HasPreviousUpdateForResource(
          global_request_id))
    return 0;

  // If the resource had already started loading, and is now labeled as an ad,
  // but was not before, we need to account for all the previously received
  // bytes.
  auto const& previous_update =
      GetDelegate().GetResourceTracker().GetPreviousUpdateForResource(
          global_request_id);
  bool is_new_ad = !previous_update->reported_as_ad_resource;
  return is_new_ad ? resource->received_data_length - resource->delta_bytes : 0;
}

void AdsPageLoadMetricsObserver::ProcessResourceForPage(
    int process_id,
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  auto mime_type = FrameData::GetResourceMimeType(resource);
  int unaccounted_ad_bytes = GetUnaccountedAdBytes(process_id, resource);
  aggregate_frame_data_->ProcessResourceLoadInFrame(
      resource, process_id, GetDelegate().GetResourceTracker());
  if (unaccounted_ad_bytes)
    aggregate_frame_data_->AdjustAdBytes(unaccounted_ad_bytes, mime_type);
  if (resource->is_main_frame_resource) {
    main_frame_data_->ProcessResourceLoadInFrame(
        resource, process_id, GetDelegate().GetResourceTracker());
    if (unaccounted_ad_bytes)
      main_frame_data_->AdjustAdBytes(unaccounted_ad_bytes, mime_type);
  }
}

void AdsPageLoadMetricsObserver::ProcessResourceForFrame(
    content::RenderFrameHost* render_frame_host,
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  const auto& id_and_data =
      ad_frames_data_.find(render_frame_host->GetFrameTreeNodeId());
  if (id_and_data == ad_frames_data_.end()) {
    if (resource->is_primary_frame_resource) {
      // Only hold onto primary resources if their load has finished, otherwise
      // we will receive a future update for them if the navigation finishes.
      if (!resource->is_complete)
        return;

      // This resource request is the primary resource load for a frame that
      // hasn't yet finished navigating. Hang onto the request info and replay
      // it once the frame finishes navigating.
      ongoing_navigation_resources_.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(render_frame_host->GetFrameTreeNodeId()),
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

  // Determine if the frame (or its ancestor) is an ad, if so attribute the
  // bytes to the highest ad ancestor.
  if (id_and_data->second == ad_frames_data_storage_.end())
    return;

  FrameData* ancestor_data = &*id_and_data->second;
  if (!ancestor_data)
    return;

  auto mime_type = FrameData::GetResourceMimeType(resource);
  int unaccounted_ad_bytes =
      GetUnaccountedAdBytes(render_frame_host->GetProcess()->GetID(), resource);
  if (unaccounted_ad_bytes)
    ancestor_data->AdjustAdBytes(unaccounted_ad_bytes, mime_type);
  ancestor_data->ProcessResourceLoadInFrame(
      resource, render_frame_host->GetProcess()->GetID(),
      GetDelegate().GetResourceTracker());
  MaybeTriggerHeavyAdIntervention(render_frame_host, ancestor_data);
}

void AdsPageLoadMetricsObserver::RecordPageResourceTotalHistograms(
    ukm::SourceId source_id) {
  // Only records histograms on pages that have some ad bytes.
  if (aggregate_frame_data_->ad_bytes() == 0)
    return;
  PAGE_BYTES_HISTOGRAM("PageLoad.Clients.Ads.Resources.Bytes.Ads2",
                       aggregate_frame_data_->ad_network_bytes());
  auto* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::AdPageLoad builder(source_id);
  builder.SetTotalBytes(aggregate_frame_data_->network_bytes() >> 10)
      .SetAdBytes(aggregate_frame_data_->ad_network_bytes() >> 10)
      .SetAdJavascriptBytes(aggregate_frame_data_->GetAdNetworkBytesForMime(
                                FrameData::ResourceMimeType::kJavascript) >>
                            10)
      .SetAdVideoBytes(aggregate_frame_data_->GetAdNetworkBytesForMime(
                           FrameData::ResourceMimeType::kVideo) >>
                       10);

  // Record cpu metrics for the page.
  builder.SetAdCpuTime(
      aggregate_ad_info_by_visibility_
          [static_cast<int>(FrameData::FrameVisibility::kAnyVisibility)]
              .cpu_time.InMilliseconds());

  base::TimeTicks current_time = clock_->NowTicks();
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
  // Record per-frame metrics for any existing frames.
  for (const auto& frame_data : ad_frames_data_storage_) {
    RecordPerFrameHistogramsForAdTagging(frame_data);
    RecordPerFrameHistogramsForCpuUsage(frame_data);
    frame_data.RecordAdFrameLoadUkmEvent(source_id);
  }

  // Clear the frame data now that all per frame metrics are recorded.
  ad_frames_data_storage_.clear();
  ad_frames_data_.clear();

  RecordAggregateHistogramsForAdTagging(
      FrameData::FrameVisibility::kNonVisible);
  RecordAggregateHistogramsForAdTagging(FrameData::FrameVisibility::kVisible);
  RecordAggregateHistogramsForAdTagging(
      FrameData::FrameVisibility::kAnyVisibility);
  RecordAggregateHistogramsForCpuUsage();
  RecordPageResourceTotalHistograms(source_id);
}

void AdsPageLoadMetricsObserver::RecordPerFrameHistogramsForCpuUsage(
    const FrameData& ad_frame_data) {
  // Get the relevant durations, set pre-interactive if the page never hit it.
  base::TimeDelta total_duration =
      GetDelegate().GetVisibilityTracker().GetForegroundDuration();
  if (time_interactive_.is_null())
    pre_interactive_duration_ = total_duration;
  base::TimeDelta post_interactive_duration =
      total_duration - pre_interactive_duration_;
  DCHECK(total_duration >= base::TimeDelta());
  DCHECK(pre_interactive_duration_ >= base::TimeDelta());
  DCHECK(post_interactive_duration >= base::TimeDelta());

  // This aggregate gets reported regardless of whether the frame used bytes.
  aggregate_ad_info_by_visibility_
      [static_cast<int>(FrameData::FrameVisibility::kAnyVisibility)]
          .cpu_time += ad_frame_data.GetTotalCpuUsage();

  if (ad_frame_data.bytes() == 0)
    return;

  // Record per frame histograms to the appropriate visibility prefixes.
  for (const auto visibility : {FrameData::FrameVisibility::kAnyVisibility,
                                ad_frame_data.visibility()}) {
    // Report the peak windowed usage, which is independent of activation status
    // (measured only for the unactivated period).  Only reported if there was a
    // relevant unactivated period.
    if ((ad_frame_data.user_activation_status() ==
             FrameData::UserActivationStatus::kNoActivation &&
         total_duration.InMilliseconds() > 0) ||
        (ad_frame_data.user_activation_status() ==
             FrameData::UserActivationStatus::kReceivedActivation &&
         ad_frame_data.pre_activation_foreground_duration().InMilliseconds() >
             0)) {
      ADS_HISTOGRAM("Cpu.AdFrames.PerFrame.PeakWindowedPercent",
                    UMA_HISTOGRAM_PERCENTAGE, visibility,
                    ad_frame_data.peak_windowed_cpu_percent());
      if (ad_frame_data.peak_window_start_time()) {
        ADS_HISTOGRAM("Cpu.AdFrames.PerFrame.PeakWindowStartTime",
                      PAGE_LOAD_HISTOGRAM, visibility,
                      ad_frame_data.peak_window_start_time().value() -
                          GetDelegate().GetNavigationStart());
      }
    }

    if (ad_frame_data.user_activation_status() ==
        FrameData::UserActivationStatus::kNoActivation) {
      base::TimeDelta task_duration_pre = ad_frame_data.GetInteractiveCpuUsage(
          FrameData::InteractiveStatus::kPreInteractive);
      base::TimeDelta task_duration_post = ad_frame_data.GetInteractiveCpuUsage(
          FrameData::InteractiveStatus::kPostInteractive);
      base::TimeDelta task_duration_total =
          task_duration_pre + task_duration_post;
      if (total_duration.InMilliseconds() > 0) {
        ADS_HISTOGRAM("Cpu.AdFrames.PerFrame.TotalUsage.Unactivated",
                      PAGE_LOAD_HISTOGRAM, visibility, task_duration_total);
      }
      if (pre_interactive_duration_.InMilliseconds() > 0) {
        ADS_HISTOGRAM(
            "Cpu.AdFrames.PerFrame.TotalUsage.Unactivated.PreInteractive",
            PAGE_LOAD_HISTOGRAM, visibility, task_duration_pre);
      }
      if (post_interactive_duration.InMilliseconds() > 0) {
        ADS_HISTOGRAM(
            "Cpu.AdFrames.PerFrame.TotalUsage.Unactivated.PostInteractive",
            PAGE_LOAD_HISTOGRAM, visibility, task_duration_post);
      }
    } else {
      base::TimeDelta task_duration_pre = ad_frame_data.GetActivationCpuUsage(
          FrameData::UserActivationStatus::kNoActivation);
      base::TimeDelta task_duration_post = ad_frame_data.GetActivationCpuUsage(
          FrameData::UserActivationStatus::kReceivedActivation);
      base::TimeDelta task_duration_total =
          task_duration_pre + task_duration_post;
      base::TimeDelta pre_activation_duration =
          ad_frame_data.pre_activation_foreground_duration();
      base::TimeDelta post_activation_duration =
          total_duration - pre_activation_duration;
      if (total_duration.InMilliseconds() > 0) {
        ADS_HISTOGRAM("Cpu.AdFrames.PerFrame.TotalUsage.Activated",
                      PAGE_LOAD_HISTOGRAM, visibility, task_duration_total);
      }
      if (pre_activation_duration.InMilliseconds() > 0) {
        ADS_HISTOGRAM(
            "Cpu.AdFrames.PerFrame.TotalUsage.Activated.PreActivation",
            PAGE_LOAD_HISTOGRAM, visibility, task_duration_pre);
      }
      if (post_activation_duration.InMilliseconds() > 0) {
        ADS_HISTOGRAM(
            "Cpu.AdFrames.PerFrame.TotalUsage.Activated.PostActivation",
            PAGE_LOAD_HISTOGRAM, visibility, task_duration_post);
      }
    }
  }
}

void AdsPageLoadMetricsObserver::RecordAggregateHistogramsForCpuUsage() {
  // If the page has an ad with the relevant visibility and non-zero bytes.
  if (aggregate_ad_info_by_visibility_
          [static_cast<int>(FrameData::FrameVisibility::kAnyVisibility)]
              .num_frames == 0)
    return;

  // Get the relevant durations, set pre-interactive if the page never hit it.
  base::TimeDelta total_duration =
      GetDelegate().GetVisibilityTracker().GetForegroundDuration();
  if (time_interactive_.is_null())
    pre_interactive_duration_ = total_duration;

  base::TimeDelta post_interactive_duration =
      total_duration - pre_interactive_duration_;
  DCHECK(total_duration >= base::TimeDelta());
  DCHECK(pre_interactive_duration_ >= base::TimeDelta());
  DCHECK(post_interactive_duration >= base::TimeDelta());

  // Only record cpu usage aggregate data for the AnyVisibility suffix as these
  // numbers do not change for different visibility types.
  FrameData::FrameVisibility visibility =
      FrameData::FrameVisibility::kAnyVisibility;

  // Record the aggregate data, which is never considered activated.
  base::TimeDelta task_duration_pre =
      aggregate_frame_data_->GetInteractiveCpuUsage(
          FrameData::InteractiveStatus::kPreInteractive);
  base::TimeDelta task_duration_post =
      aggregate_frame_data_->GetInteractiveCpuUsage(
          FrameData::InteractiveStatus::kPostInteractive);
  base::TimeDelta task_duration_total = task_duration_pre + task_duration_post;
  if (total_duration.InMilliseconds() > 0) {
    ADS_HISTOGRAM("Cpu.FullPage.TotalUsage", PAGE_LOAD_HISTOGRAM, visibility,
                  task_duration_total);
    ADS_HISTOGRAM("Cpu.FullPage.PeakWindowedPercent", UMA_HISTOGRAM_PERCENTAGE,
                  visibility,
                  aggregate_frame_data_->peak_windowed_cpu_percent());
    if (aggregate_frame_data_->peak_window_start_time()) {
      ADS_HISTOGRAM("Cpu.FullPage.PeakWindowStartTime", PAGE_LOAD_HISTOGRAM,
                    visibility,
                    aggregate_frame_data_->peak_window_start_time().value() -
                        GetDelegate().GetNavigationStart());
    }
  }
  if (pre_interactive_duration_.InMilliseconds() > 0) {
    ADS_HISTOGRAM("Cpu.FullPage.TotalUsage.PreInteractive", PAGE_LOAD_HISTOGRAM,
                  visibility, task_duration_pre);
  }
  if (post_interactive_duration.InMilliseconds() > 0) {
    ADS_HISTOGRAM("Cpu.FullPage.TotalUsage.PostInteractive",
                  PAGE_LOAD_HISTOGRAM, visibility, task_duration_post);
  }
}

void AdsPageLoadMetricsObserver::RecordPerFrameHistogramsForAdTagging(
    const FrameData& ad_frame_data) {
  if (ad_frame_data.bytes() == 0)
    return;

  // Record per frame histograms to the appropriate visibility prefixes.
  for (const auto visibility : {FrameData::FrameVisibility::kAnyVisibility,
                                ad_frame_data.visibility()}) {
    // Update aggregate ad information.
    aggregate_ad_info_by_visibility_[static_cast<int>(visibility)].bytes +=
        ad_frame_data.bytes();
    aggregate_ad_info_by_visibility_[static_cast<int>(visibility)]
        .network_bytes += ad_frame_data.network_bytes();
    aggregate_ad_info_by_visibility_[static_cast<int>(visibility)].num_frames +=
        1;

    int frame_area = ad_frame_data.frame_size().GetCheckedArea().ValueOrDefault(
        std::numeric_limits<int>::max());
    ADS_HISTOGRAM("FrameCounts.AdFrames.PerFrame.SqrtNumberOfPixels",
                  UMA_HISTOGRAM_COUNTS_10000, visibility,
                  std::sqrt(frame_area));
    ADS_HISTOGRAM("FrameCounts.AdFrames.PerFrame.SmallestDimension",
                  UMA_HISTOGRAM_COUNTS_10000, visibility,
                  std::min(ad_frame_data.frame_size().width(),
                           ad_frame_data.frame_size().height()));

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
    ADS_HISTOGRAM("HeavyAds.ComputedType2", UMA_HISTOGRAM_ENUMERATION,
                  visibility, ad_frame_data.heavy_ad_status());
  }
}

void AdsPageLoadMetricsObserver::RecordAggregateHistogramsForAdTagging(
    FrameData::FrameVisibility visibility) {
  if (aggregate_frame_data_->bytes() == 0)
    return;

  const auto& aggregate_ad_info =
      aggregate_ad_info_by_visibility_[static_cast<int>(visibility)];

  // TODO(ericrobinson): Consider renaming this to match
  //   'FrameCounts.AdFrames.PerFrame.OriginStatus'.
  ADS_HISTOGRAM("FrameCounts.AnyParentFrame.AdFrames",
                UMA_HISTOGRAM_COUNTS_1000, visibility,
                aggregate_ad_info.num_frames);

  // Don't post UMA for pages that don't have ads.
  if (aggregate_ad_info.num_frames == 0)
    return;

  ADS_HISTOGRAM("Bytes.NonAdFrames.Aggregate.Total", PAGE_BYTES_HISTOGRAM,
                visibility,
                aggregate_frame_data_->bytes() - aggregate_ad_info.bytes);

  ADS_HISTOGRAM("Bytes.FullPage.Total", PAGE_BYTES_HISTOGRAM, visibility,
                aggregate_frame_data_->bytes());
  ADS_HISTOGRAM("Bytes.FullPage.Network", PAGE_BYTES_HISTOGRAM, visibility,
                aggregate_frame_data_->network_bytes());

  if (aggregate_frame_data_->bytes()) {
    ADS_HISTOGRAM(
        "Bytes.FullPage.Total.PercentAds", UMA_HISTOGRAM_PERCENTAGE, visibility,
        aggregate_ad_info.bytes * 100 / aggregate_frame_data_->bytes());
  }
  if (aggregate_frame_data_->network_bytes()) {
    ADS_HISTOGRAM("Bytes.FullPage.Network.PercentAds", UMA_HISTOGRAM_PERCENTAGE,
                  visibility,
                  aggregate_ad_info.network_bytes * 100 /
                      aggregate_frame_data_->network_bytes());
  }

  ADS_HISTOGRAM("Bytes.AdFrames.Aggregate.Total", PAGE_BYTES_HISTOGRAM,
                visibility, aggregate_ad_info.bytes);
  ADS_HISTOGRAM("Bytes.AdFrames.Aggregate.Network", PAGE_BYTES_HISTOGRAM,
                visibility, aggregate_ad_info.network_bytes);

  if (aggregate_ad_info.bytes) {
    ADS_HISTOGRAM(
        "Bytes.AdFrames.Aggregate.PercentNetwork", UMA_HISTOGRAM_PERCENTAGE,
        visibility,
        aggregate_ad_info.network_bytes * 100 / aggregate_ad_info.bytes);
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
    content::RenderFrameHost* rfh) {
  if (!rfh)
    return;
  const auto& frame_id_and_request =
      ongoing_navigation_resources_.find(rfh->GetFrameTreeNodeId());
  if (frame_id_and_request == ongoing_navigation_resources_.end())
    return;
  ProcessResourceForFrame(rfh, frame_id_and_request->second);
  ongoing_navigation_resources_.erase(frame_id_and_request);
}

FrameData* AdsPageLoadMetricsObserver::FindFrameData(FrameTreeNodeId id) {
  const auto& id_and_data = ad_frames_data_.find(id);
  if (id_and_data == ad_frames_data_.end())
    return nullptr;

  // If the iterator is not valid, this FrameTreeNodeId is not associated with
  // an ad.
  if (id_and_data->second == ad_frames_data_storage_.end())
    return nullptr;

  return &*id_and_data->second;
}

bool AdsPageLoadMetricsObserver::IsBlocklisted() {
  if (!heavy_ad_blocklist_enabled_)
    return false;

  if (heavy_ads_blocklist_blocklisted_)
    return true;

  auto* blocklist = GetHeavyAdBlocklist();

  // Treat instances where the blocklist is unavailable as blocklisted. This
  // includes incognito profiles, which do not create a blocklist service.
  if (!blocklist) {
    heavy_ads_blocklist_blocklisted_ = true;
    return true;
  }

  std::vector<blacklist::BlacklistReason> passed_reasons;
  auto blocklist_reason = blocklist->IsLoadedAndAllowed(
      GetDelegate().GetWebContents()->GetLastCommittedURL().host(),
      static_cast<int>(HeavyAdBlocklistType::kHeavyAdOnlyType),
      false /* opt_out */, &passed_reasons);
  heavy_ads_blocklist_blocklisted_ =
      (blocklist_reason != blacklist::BlacklistReason::kAllowed);

  // TODO(https://crbug.com/959849): Add UMA for blocklist reasons.

  return heavy_ads_blocklist_blocklisted_;
}

HeavyAdBlocklist* AdsPageLoadMetricsObserver::GetHeavyAdBlocklist() {
  if (heavy_ad_blocklist_)
    return heavy_ad_blocklist_;
  auto* heavy_ad_service = HeavyAdServiceFactory::GetForBrowserContext(
      GetDelegate().GetWebContents()->GetBrowserContext());
  if (!heavy_ad_service)
    return nullptr;

  return heavy_ad_service->heavy_ad_blocklist();
}
