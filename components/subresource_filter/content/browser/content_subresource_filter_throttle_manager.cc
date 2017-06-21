// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/subresource_filter/content/browser/activation_state_computing_navigation_throttle.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/content/browser/page_load_statistics.h"
#include "components/subresource_filter/content/browser/subframe_navigation_filtering_throttle.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/console_message_level.h"
#include "net/base/net_errors.h"

namespace subresource_filter {

bool ContentSubresourceFilterThrottleManager::Delegate::
    AllowStrongPopupBlocking() {
  return false;
}

ContentSubresourceFilterThrottleManager::
    ContentSubresourceFilterThrottleManager(
        Delegate* delegate,
        VerifiedRulesetDealer::Handle* dealer_handle,
        content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      scoped_observer_(this),
      dealer_handle_(dealer_handle),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  SubresourceFilterObserverManager::CreateForWebContents(web_contents);
  scoped_observer_.Add(
      SubresourceFilterObserverManager::FromWebContents(web_contents));
}

ContentSubresourceFilterThrottleManager::
    ~ContentSubresourceFilterThrottleManager() {}

void ContentSubresourceFilterThrottleManager::OnSubresourceFilterGoingAway() {
  // Stop observing here because the observer manager could be destroyed by the
  // time this class is destroyed.
  scoped_observer_.RemoveAll();
}

void ContentSubresourceFilterThrottleManager::RenderFrameDeleted(
    content::RenderFrameHost* frame_host) {
  activated_frame_hosts_.erase(frame_host);
  DestroyRulesetHandleIfNoLongerUsed();
}

// Pull the AsyncDocumentSubresourceFilter and its associated ActivationState
// out of the activation state computing throttle. Store it for later filtering
// of subframe navigations.
void ContentSubresourceFilterThrottleManager::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetNetErrorCode() != net::OK)
    return;

  auto throttle = ongoing_activation_throttles_.find(navigation_handle);
  if (throttle == ongoing_activation_throttles_.end())
    return;

  // Main frame throttles with disabled page-level activation will not have
  // associated filters.
  AsyncDocumentSubresourceFilter* filter = throttle->second->filter();
  if (!filter)
    return;

  // A filter with DISABLED activation indicates a corrupted ruleset.
  ActivationLevel level = filter->activation_state().activation_level;
  if (level == ActivationLevel::DISABLED)
    return;

  TRACE_EVENT1(
      TRACE_DISABLED_BY_DEFAULT("loading"),
      "ContentSubresourceFilterThrottleManager::ReadyToCommitNavigation",
      "activation_state", filter->activation_state().ToTracedValue());

  throttle->second->WillSendActivationToRenderer();

  content::RenderFrameHost* frame_host =
      navigation_handle->GetRenderFrameHost();
  frame_host->Send(new SubresourceFilterMsg_ActivateForNextCommittedLoad(
      frame_host->GetRoutingID(), filter->activation_state()));
}

void ContentSubresourceFilterThrottleManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Do nothing if the navigation finished in the same document. Just make sure
  // to not leak throttle pointers.
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    ongoing_activation_throttles_.erase(navigation_handle);
    return;
  }

  auto throttle = ongoing_activation_throttles_.find(navigation_handle);
  std::unique_ptr<AsyncDocumentSubresourceFilter> filter;
  if (throttle != ongoing_activation_throttles_.end()) {
    filter = throttle->second->ReleaseFilter();
    ongoing_activation_throttles_.erase(throttle);
  }

  content::RenderFrameHost* frame_host =
      navigation_handle->GetRenderFrameHost();
  if (navigation_handle->IsInMainFrame()) {
    current_committed_load_has_notified_disallowed_load_ = false;
    statistics_.reset();
    if (filter) {
      statistics_ =
          base::MakeUnique<PageLoadStatistics>(filter->activation_state());
      if (filter->activation_state().enable_logging) {
        DCHECK(filter->activation_state().activation_level !=
               ActivationLevel::DISABLED);
        frame_host->AddMessageToConsole(content::CONSOLE_MESSAGE_LEVEL_WARNING,
                                        kActivationConsoleMessage);
      }
    }
    ActivationLevel level = filter ? filter->activation_state().activation_level
                                   : ActivationLevel::DISABLED;
    UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.PageLoad.ActivationState",
                              level, ActivationLevel::LAST);
  }

  // Make sure |activated_frame_hosts_| is updated or cleaned up depending on
  // this navigation's activation state.
  if (filter) {
    base::OnceClosure disallowed_callback(base::BindOnce(
        &ContentSubresourceFilterThrottleManager::MaybeCallFirstDisallowedLoad,
        weak_ptr_factory_.GetWeakPtr()));
    filter->set_first_disallowed_load_callback(std::move(disallowed_callback));
    activated_frame_hosts_[frame_host] = std::move(filter);
  } else {
    activated_frame_hosts_.erase(frame_host);
  }
  DestroyRulesetHandleIfNoLongerUsed();
}

void ContentSubresourceFilterThrottleManager::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!statistics_ || render_frame_host->GetParent())
    return;
  statistics_->OnDidFinishLoad();
}

bool ContentSubresourceFilterThrottleManager::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ContentSubresourceFilterThrottleManager, message)
    IPC_MESSAGE_HANDLER(SubresourceFilterHostMsg_DidDisallowFirstSubresource,
                        MaybeCallFirstDisallowedLoad)
    IPC_MESSAGE_HANDLER(SubresourceFilterHostMsg_DocumentLoadStatistics,
                        OnDocumentLoadStatistics)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// Sets the desired page-level |activation_state| for the currently ongoing
// page load, identified by its main-frame |navigation_handle|. If this method
// is not called for a main-frame navigation, the default behavior is no
// activation for that page load.
void ContentSubresourceFilterThrottleManager::OnPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    ActivationDecision activation_decision,
    const ActivationState& activation_state) {
  DCHECK(navigation_handle->IsInMainFrame());
  DCHECK(!navigation_handle->HasCommitted());
  // Do not notify the throttle if activation is disabled.
  if (activation_state.activation_level == ActivationLevel::DISABLED)
    return;
  auto it = ongoing_activation_throttles_.find(navigation_handle);
  if (it != ongoing_activation_throttles_.end()) {
    it->second->NotifyPageActivationWithRuleset(EnsureRulesetHandle(),
                                                activation_state);
  }
}

void ContentSubresourceFilterThrottleManager::MaybeAppendNavigationThrottles(
    content::NavigationHandle* navigation_handle,
    std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles) {
  DCHECK(!navigation_handle->IsSameDocument());
  if (!dealer_handle_)
    return;
  if (auto filtering_throttle =
          MaybeCreateSubframeNavigationFilteringThrottle(navigation_handle)) {
    throttles->push_back(std::move(filtering_throttle));
  }
  if (auto activation_throttle =
          MaybeCreateActivationStateComputingThrottle(navigation_handle)) {
    ongoing_activation_throttles_[navigation_handle] =
        activation_throttle.get();
    throttles->push_back(std::move(activation_throttle));
  }
}

bool ContentSubresourceFilterThrottleManager::ShouldDisallowNewWindow(
    const content::OpenURLParams* open_url_params) {
  auto it = activated_frame_hosts_.find(web_contents()->GetMainFrame());
  if (it == activated_frame_hosts_.end())
    return false;
  const ActivationState state = it->second->activation_state();
  if (state.activation_level != ActivationLevel::ENABLED ||
      state.filtering_disabled_for_document ||
      state.generic_blocking_rules_disabled ||
      !delegate_->AllowStrongPopupBlocking()) {
    return false;
  }

  // It is very tricky to filter out popups from OpenURLFromTab. For right now,
  // just allow all of them. |open_url_params| will be nullptr if it is coming
  // from window.open, which should remain blocked.
  //
  // TODO(csharrison): Add additional parameters to OpenURLParams like a trusted
  // bit to determined if the event that led to this was issued via JS.
  if (open_url_params)
    return false;

  // This should trigger the standard popup blocking UI, so don't force the
  // subresource filter specific UI here.
  return true;
}

std::unique_ptr<SubframeNavigationFilteringThrottle>
ContentSubresourceFilterThrottleManager::
    MaybeCreateSubframeNavigationFilteringThrottle(
        content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame())
    return nullptr;
  AsyncDocumentSubresourceFilter* parent_filter =
      GetParentFrameFilter(navigation_handle);
  return parent_filter ? base::MakeUnique<SubframeNavigationFilteringThrottle>(
                             navigation_handle, parent_filter)
                       : nullptr;
}

std::unique_ptr<ActivationStateComputingNavigationThrottle>
ContentSubresourceFilterThrottleManager::
    MaybeCreateActivationStateComputingThrottle(
        content::NavigationHandle* navigation_handle) {
  // Main frames: create unconditionally.
  if (navigation_handle->IsInMainFrame()) {
    return ActivationStateComputingNavigationThrottle::CreateForMainFrame(
        navigation_handle);
  }

  // Subframes: create only for frames with activated parents.
  AsyncDocumentSubresourceFilter* parent_filter =
      GetParentFrameFilter(navigation_handle);
  if (!parent_filter)
    return nullptr;
  DCHECK(ruleset_handle_);
  return ActivationStateComputingNavigationThrottle::CreateForSubframe(
      navigation_handle, ruleset_handle_.get(),
      parent_filter->activation_state());
}

AsyncDocumentSubresourceFilter*
ContentSubresourceFilterThrottleManager::GetParentFrameFilter(
    content::NavigationHandle* child_frame_navigation) {
  DCHECK(!child_frame_navigation->IsInMainFrame());
  content::RenderFrameHost* parent = child_frame_navigation->GetParentFrame();
  DCHECK(parent);
  auto it = activated_frame_hosts_.find(parent);
  return it == activated_frame_hosts_.end() ? nullptr : it->second.get();
}

void ContentSubresourceFilterThrottleManager::MaybeCallFirstDisallowedLoad() {
  if (current_committed_load_has_notified_disallowed_load_)
    return;
  delegate_->OnFirstSubresourceLoadDisallowed();
  current_committed_load_has_notified_disallowed_load_ = true;
}

VerifiedRuleset::Handle*
ContentSubresourceFilterThrottleManager::EnsureRulesetHandle() {
  if (!ruleset_handle_)
    ruleset_handle_ = base::MakeUnique<VerifiedRuleset::Handle>(dealer_handle_);
  return ruleset_handle_.get();
}

void ContentSubresourceFilterThrottleManager::
    DestroyRulesetHandleIfNoLongerUsed() {
  if (activated_frame_hosts_.size() + ongoing_activation_throttles_.size() ==
      0u) {
    ruleset_handle_.reset();
  }
}

void ContentSubresourceFilterThrottleManager::OnDocumentLoadStatistics(
    const DocumentLoadStatistics& statistics) {
  if (statistics_)
    statistics_->OnDocumentLoadStatistics(statistics);
}

}  // namespace subresource_filter
