// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/back_forward_cache_impl.h"

#include <unordered_set>

#include "base/strings/string_util.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/page_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/common/navigation_policy.h"
#include "net/http/http_status_code.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {

namespace {

using blink::scheduler::WebSchedulerTrackedFeature;

// The number of pages the BackForwardCache can hold per tab.
static constexpr size_t kBackForwardCacheLimit = 1;

// The default time to live in seconds for documents in BackForwardCache.
static constexpr int kDefaultTimeToLiveInBackForwardCacheInSeconds = 15;

// Converts a WebSchedulerTrackedFeature to a bit for use in a bitmask.
constexpr uint64_t ToFeatureBit(WebSchedulerTrackedFeature feature) {
  return 1 << static_cast<uint32_t>(feature);
}

void SetPageFrozenImpl(
    RenderFrameHostImpl* render_frame_host,
    bool frozen,
    std::unordered_set<RenderViewHostImpl*>* render_view_hosts) {
  RenderViewHostImpl* render_view_host = render_frame_host->render_view_host();
  // (Un)Freeze the frame's page if it is not (un)frozen yet.
  if (render_view_hosts->find(render_view_host) == render_view_hosts->end()) {
    // The state change for bfcache is:
    // PageHidden -> PageFrozen -> PageResumed -> PageShown.
    //
    // See: https://developers.google.com/web/updates/2018/07/page-lifecycle-api
    int rvh_routing_id = render_view_host->GetRoutingID();
    if (frozen) {
      render_view_host->Send(
          new PageMsg_PutPageIntoBackForwardCache(rvh_routing_id));
    } else {
      render_view_host->Send(
          new PageMsg_RestorePageFromBackForwardCache(rvh_routing_id));
    }
    render_view_hosts->insert(render_view_host);
  }
  // Recurse on |render_frame_host|'s children.
  for (size_t index = 0; index < render_frame_host->child_count(); ++index) {
    RenderFrameHostImpl* child_frame_host =
        render_frame_host->child_at(index)->current_frame_host();
    SetPageFrozenImpl(child_frame_host, frozen, render_view_hosts);
  }
}

bool IsServiceWorkerSupported() {
  static constexpr base::FeatureParam<bool> service_worker_supported(
      &features::kBackForwardCache, "service_worker_supported", false);
  return service_worker_supported.Get();
}

uint64_t GetDisallowedFeatures() {
  // TODO(lowell): Finalize disallowed feature list, and test for each
  // disallowed feature.
  constexpr uint64_t kAlwaysDisallowedFeatures =
      ToFeatureBit(WebSchedulerTrackedFeature::kWebRTC) |
      ToFeatureBit(WebSchedulerTrackedFeature::kContainsPlugins) |
      ToFeatureBit(WebSchedulerTrackedFeature::kDedicatedWorkerOrWorklet) |
      ToFeatureBit(
          WebSchedulerTrackedFeature::kOutstandingIndexedDBTransaction) |
      ToFeatureBit(
          WebSchedulerTrackedFeature::kHasScriptableFramesInMultipleTabs) |
      ToFeatureBit(
          WebSchedulerTrackedFeature::kRequestedNotificationsPermission) |
      ToFeatureBit(WebSchedulerTrackedFeature::kRequestedMIDIPermission) |
      ToFeatureBit(
          WebSchedulerTrackedFeature::kRequestedAudioCapturePermission) |
      ToFeatureBit(
          WebSchedulerTrackedFeature::kRequestedVideoCapturePermission) |
      ToFeatureBit(WebSchedulerTrackedFeature::kRequestedSensorsPermission) |
      ToFeatureBit(
          WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission) |
      ToFeatureBit(WebSchedulerTrackedFeature::kBroadcastChannel) |
      ToFeatureBit(WebSchedulerTrackedFeature::kIndexedDBConnection) |
      ToFeatureBit(WebSchedulerTrackedFeature::kWebGL) |
      ToFeatureBit(WebSchedulerTrackedFeature::kWebVR) |
      ToFeatureBit(WebSchedulerTrackedFeature::kWebXR);

  uint64_t result = kAlwaysDisallowedFeatures;

  if (!IsServiceWorkerSupported()) {
    result |=
        ToFeatureBit(WebSchedulerTrackedFeature::kServiceWorkerControlledPage);
  }
  return result;
}

std::string DescribeFeatures(uint64_t blocklisted_features) {
  std::vector<std::string> features;
  for (size_t i = 0;
       i <= static_cast<size_t>(WebSchedulerTrackedFeature::kMaxValue); ++i) {
    if (blocklisted_features & (1 << i)) {
      features.push_back(blink::scheduler::FeatureToString(
          static_cast<WebSchedulerTrackedFeature>(i)));
    }
  }
  return base::JoinString(features, ", ");
}

}  // namespace

enum class BackForwardCacheImpl::CanStoreDocumentResult::Reason : uint8_t {
  kNotMainFrame,
  kBackForwardCacheDisabled,
  kRelatedActiveContentsExist,
  kHTTPStatusNotOK,
  kSchemeNotHTTPOrHTTPS,
  kLoading,
  kWasGrantedMediaAccess,
  kBlocklistedFeatures,
  kDisableForRenderFrameHostCalled
};

std::string BackForwardCacheImpl::CanStoreDocumentResult::ToString() {
  if (can_store)
    return "Yes";

  switch (reason.value()) {
    case Reason::kNotMainFrame:
      return "No: not a main frame";
    case Reason::kBackForwardCacheDisabled:
      return "No: BackForwardCache disabled";
    case Reason::kRelatedActiveContentsExist:
      return "No: related active contents exist";
    case Reason::kHTTPStatusNotOK:
      return "No: HTTP status is not OK";
    case Reason::kSchemeNotHTTPOrHTTPS:
      return "No: scheme is not HTTP or HTTPS";
    case Reason::kLoading:
      return "No: frame is not fully loaded";
    case Reason::kWasGrantedMediaAccess:
      return "No: frame was granted microphone or camera access";
    case Reason::kBlocklistedFeatures:
      return "No: blocklisted features: " +
             DescribeFeatures(blocklisted_features);
    case Reason::kDisableForRenderFrameHostCalled:
      return "No: BackForwardCache::DisableForRenderFrameHost() was called";
  }
}

BackForwardCacheImpl::CanStoreDocumentResult::CanStoreDocumentResult(
    const CanStoreDocumentResult&) = default;
BackForwardCacheImpl::CanStoreDocumentResult::~CanStoreDocumentResult() =
    default;

BackForwardCacheImpl::CanStoreDocumentResult
BackForwardCacheImpl::CanStoreDocumentResult::Yes() {
  return CanStoreDocumentResult(true, base::nullopt, 0);
}

BackForwardCacheImpl::CanStoreDocumentResult
BackForwardCacheImpl::CanStoreDocumentResult::No(Reason reason) {
  return CanStoreDocumentResult(false, reason, 0);
}

BackForwardCacheImpl::CanStoreDocumentResult
BackForwardCacheImpl::CanStoreDocumentResult::NoDueToFeatures(
    uint64_t blocklisted_features) {
  return CanStoreDocumentResult(false, Reason::kBlocklistedFeatures,
                                blocklisted_features);
}

BackForwardCacheImpl::CanStoreDocumentResult::CanStoreDocumentResult(
    bool can_store,
    base::Optional<Reason> reason,
    uint64_t blocklisted_features)
    : can_store(can_store),
      reason(reason),
      blocklisted_features(blocklisted_features) {}

BackForwardCacheImpl::BackForwardCacheImpl() : weak_factory_(this) {}
BackForwardCacheImpl::~BackForwardCacheImpl() = default;

base::TimeDelta BackForwardCacheImpl::GetTimeToLiveInBackForwardCache() {
  return base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
      features::kBackForwardCache, "TimeToLiveInBackForwardCacheInSeconds",
      kDefaultTimeToLiveInBackForwardCacheInSeconds));
}

BackForwardCacheImpl::CanStoreDocumentResult
BackForwardCacheImpl::CanStoreDocument(RenderFrameHostImpl* rfh) {
  // Use the BackForwardCache only for the main frame.
  if (rfh->GetParent()) {
    return CanStoreDocumentResult::No(
        CanStoreDocumentResult::Reason::kNotMainFrame);
  }

  if (!IsBackForwardCacheEnabled() || is_disabled_for_testing_) {
    return CanStoreDocumentResult::No(
        CanStoreDocumentResult::Reason::kBackForwardCacheDisabled);
  }

  if (rfh->is_back_forward_cache_disallowed()) {
    return CanStoreDocumentResult::No(
        CanStoreDocumentResult::Reason::kDisableForRenderFrameHostCalled);
  }

  // Two pages in the same BrowsingInstance can script each other. When a page
  // can be scripted from outside, it can't enter the BackForwardCache.
  //
  // The "RelatedActiveContentsCount" below is compared against 0, not 1. This
  // is because the |rfh| is not a "current" RenderFrameHost anymore. It is not
  // "active" itself.
  //
  // This check makes sure the old and new document aren't sharing the same
  // BrowsingInstance.
  if (rfh->GetSiteInstance()->GetRelatedActiveContentsCount() != 0) {
    return CanStoreDocumentResult::No(
        CanStoreDocumentResult::Reason::kRelatedActiveContentsExist);
  }

  // Only store documents that have successful http status code.
  // Note that for error pages, |last_http_status_code| is equal to 0.
  if (rfh->last_http_status_code() != net::HTTP_OK) {
    return CanStoreDocumentResult::No(
        CanStoreDocumentResult::Reason::kHTTPStatusNotOK);
  }

  // Do store main document with non HTTP/HTTPS URL scheme. In particular, this
  // excludes the new tab page.
  if (!rfh->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    return CanStoreDocumentResult::No(
        CanStoreDocumentResult::Reason::kSchemeNotHTTPOrHTTPS);
  }

  return CanStoreRenderFrameHost(rfh, GetDisallowedFeatures());
}

// Recursively checks whether this RenderFrameHost and all child frames
// can be cached.
BackForwardCacheImpl::CanStoreDocumentResult
BackForwardCacheImpl::CanStoreRenderFrameHost(RenderFrameHostImpl* rfh,
                                              uint64_t disallowed_features) {
  // For the main frame, we don't check loading at the FrameTreeNode level,
  // because the FrameTreeNode has already begun loading the page being
  // navigated to.
  bool is_loading = rfh->frame_tree_node()->IsMainFrame()
                        ? rfh->is_loading()
                        : rfh->frame_tree_node()->IsLoading();
  if (is_loading)
    return CanStoreDocumentResult::No(CanStoreDocumentResult::Reason::kLoading);

  // If the rfh has ever granted media access, prevent it from entering cache.
  // TODO(crbug.com/989379): Consider only blocking when there's an active
  //                         media stream.
  if (rfh->was_granted_media_access()) {
    return CanStoreDocumentResult::No(
        CanStoreDocumentResult::Reason::kWasGrantedMediaAccess);
  }

  // Don't cache the page if it uses any disallowed features.
  // TODO(altimin): At the moment only the first detected failure is reported.
  // For reporting purposes it's a good idea to also collect this information
  // from children.
  if (uint64_t banned_features =
          disallowed_features & rfh->scheduler_tracked_features()) {
    return CanStoreDocumentResult::NoDueToFeatures(banned_features);
  }

  for (size_t i = 0; i < rfh->child_count(); i++) {
    CanStoreDocumentResult can_store_child = CanStoreRenderFrameHost(
        rfh->child_at(i)->current_frame_host(), disallowed_features);
    if (!can_store_child.can_store)
      return can_store_child;
  }

  return CanStoreDocumentResult::Yes();
}

void BackForwardCacheImpl::StoreDocument(
    std::unique_ptr<RenderFrameHostImpl> rfh) {
  TRACE_EVENT0("navigation", "BackForwardCache::StoreDocument");
  DCHECK(CanStoreDocument(rfh.get()));

  rfh->EnterBackForwardCache();
  render_frame_hosts_.push_front(std::move(rfh));

  size_t size_limit = cache_size_limit_for_testing_
                          ? cache_size_limit_for_testing_
                          : kBackForwardCacheLimit;
  // Evict the least recently used documents if the BackForwardCache list is
  // full.
  size_t available_count = 0;
  for (auto& frame_host : render_frame_hosts_) {
    if (frame_host->is_evicted_from_back_forward_cache())
      continue;
    if (++available_count > size_limit)
      frame_host->EvictFromBackForwardCache();
  }
}

void BackForwardCacheImpl::Freeze(RenderFrameHostImpl* main_rfh) {
  // Several RenderFrameHost can live under the same RenderViewHost.
  // |frozen_render_view_hosts| keeps track of the ones that freezing has been
  // applied to.
  std::unordered_set<RenderViewHostImpl*> frozen_render_view_hosts;
  SetPageFrozenImpl(main_rfh, /*frozen = */ true, &frozen_render_view_hosts);
}

void BackForwardCacheImpl::Resume(RenderFrameHostImpl* main_rfh) {
  // |unfrozen_render_view_hosts| keeps track of the ones that resuming has
  // been applied to.
  std::unordered_set<RenderViewHostImpl*> unfrozen_render_view_hosts;
  SetPageFrozenImpl(main_rfh, /*frozen = */ false, &unfrozen_render_view_hosts);
}

std::unique_ptr<RenderFrameHostImpl> BackForwardCacheImpl::RestoreDocument(
    int navigation_entry_id) {
  TRACE_EVENT0("navigation", "BackForwardCache::RestoreDocument");
  // Select the RenderFrameHostImpl matching the navigation entry.
  auto matching_rfh = std::find_if(
      render_frame_hosts_.begin(), render_frame_hosts_.end(),
      [navigation_entry_id](std::unique_ptr<RenderFrameHostImpl>& rfh) {
        return rfh->nav_entry_id() == navigation_entry_id;
      });

  // Not found.
  if (matching_rfh == render_frame_hosts_.end())
    return nullptr;

  // Don't restore an evicted frame.
  if ((*matching_rfh)->is_evicted_from_back_forward_cache())
    return nullptr;

  std::unique_ptr<RenderFrameHostImpl> rfh = std::move(*matching_rfh);
  render_frame_hosts_.erase(matching_rfh);
  rfh->LeaveBackForwardCache();
  return rfh;
}

void BackForwardCacheImpl::Flush() {
  TRACE_EVENT0("navigation", "BackForwardCache::Flush");
  render_frame_hosts_.clear();
}

void BackForwardCacheImpl::PostTaskToDestroyEvictedFrames() {
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&BackForwardCacheImpl::DestroyEvictedFrames,
                                weak_factory_.GetWeakPtr()));
}

void BackForwardCacheImpl::DisableForRenderFrameHost(GlobalFrameRoutingId id,
                                                     std::string_view reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* rfh = RenderFrameHostImpl::FromID(id);
  if (rfh)
    rfh->DisallowBackForwardCache();
}

void BackForwardCacheImpl::DisableForTesting(DisableForTestingReason reason) {
  is_disabled_for_testing_ = true;

  // This could happen if a test populated some pages in the cache, then
  // called DisableForTesting(). This is not something we currently expect tests
  // to do.
  DCHECK(render_frame_hosts_.empty());
}

RenderFrameHostImpl* BackForwardCacheImpl::GetDocument(
    int navigation_entry_id) {
  auto matching_rfh = std::find_if(
      render_frame_hosts_.begin(), render_frame_hosts_.end(),
      [navigation_entry_id](std::unique_ptr<RenderFrameHostImpl>& rfh) {
        return rfh->nav_entry_id() == navigation_entry_id;
      });

  if (matching_rfh == render_frame_hosts_.end())
    return nullptr;

  // Don't return the frame if it is evicted.
  if ((*matching_rfh)->is_evicted_from_back_forward_cache())
    return nullptr;

  return (*matching_rfh).get();
}

void BackForwardCacheImpl::DestroyEvictedFrames() {
  TRACE_EVENT0("navigation", "BackForwardCache::DestroyEvictedFrames");
  if (render_frame_hosts_.empty())
    return;
  render_frame_hosts_.erase(
      std::remove_if(render_frame_hosts_.begin(), render_frame_hosts_.end(),
                     [](std::unique_ptr<RenderFrameHostImpl>& rfh) {
                       return rfh->is_evicted_from_back_forward_cache();
                     }));
}
}  // namespace content
