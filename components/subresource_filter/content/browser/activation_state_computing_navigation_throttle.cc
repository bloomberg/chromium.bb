// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/activation_state_computing_navigation_throttle.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace subresource_filter {

// static
std::unique_ptr<ActivationStateComputingNavigationThrottle>
ActivationStateComputingNavigationThrottle::CreateForMainFrame(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle->IsInMainFrame());
  return base::WrapUnique(new ActivationStateComputingNavigationThrottle(
      navigation_handle, base::Optional<ActivationState>(), nullptr));
}

// static
std::unique_ptr<ActivationStateComputingNavigationThrottle>
ActivationStateComputingNavigationThrottle::CreateForSubframe(
    content::NavigationHandle* navigation_handle,
    VerifiedRuleset::Handle* ruleset_handle,
    const ActivationState& parent_activation_state) {
  DCHECK(!navigation_handle->IsInMainFrame());
  DCHECK_NE(ActivationLevel::DISABLED,
            parent_activation_state.activation_level);
  DCHECK(ruleset_handle);
  return base::WrapUnique(new ActivationStateComputingNavigationThrottle(
      navigation_handle, parent_activation_state, ruleset_handle));
}

ActivationStateComputingNavigationThrottle::
    ActivationStateComputingNavigationThrottle(
        content::NavigationHandle* navigation_handle,
        const base::Optional<ActivationState> parent_activation_state,
        VerifiedRuleset::Handle* ruleset_handle)
    : content::NavigationThrottle(navigation_handle),
      parent_activation_state_(parent_activation_state),
      ruleset_handle_(ruleset_handle),
      weak_ptr_factory_(this) {}

ActivationStateComputingNavigationThrottle::
    ~ActivationStateComputingNavigationThrottle() {}

void ActivationStateComputingNavigationThrottle::
    NotifyPageActivationWithRuleset(
        VerifiedRuleset::Handle* ruleset_handle,
        const ActivationState& page_activation_state) {
  DCHECK(navigation_handle()->IsInMainFrame());
  DCHECK(!parent_activation_state_);
  DCHECK(!activation_state_);
  DCHECK(!ruleset_handle_);
  // DISABLED implies null ruleset.
  DCHECK(page_activation_state.activation_level != ActivationLevel::DISABLED ||
         !ruleset_handle);
  parent_activation_state_.emplace(page_activation_state);
  ruleset_handle_ = ruleset_handle;
}

content::NavigationThrottle::ThrottleCheckResult
ActivationStateComputingNavigationThrottle::WillProcessResponse() {
  // Main frame navigations with disabled page-level activation become
  // pass-through throttles.
  if (!parent_activation_state_ ||
      parent_activation_state_->activation_level == ActivationLevel::DISABLED) {
    DCHECK(navigation_handle()->IsInMainFrame());
    DCHECK(!ruleset_handle_);
    activation_state_.emplace(ActivationLevel::DISABLED);
    return content::NavigationThrottle::ThrottleCheckResult::PROCEED;
  }

  DCHECK(ruleset_handle_);
  AsyncDocumentSubresourceFilter::InitializationParams params;
  params.document_url = navigation_handle()->GetURL();
  params.parent_activation_state = parent_activation_state_.value();
  if (!navigation_handle()->IsInMainFrame()) {
    content::RenderFrameHost* parent =
        navigation_handle()->GetWebContents()->FindFrameByFrameTreeNodeId(
            navigation_handle()->GetParentFrameTreeNodeId());
    DCHECK(parent);
    params.parent_document_origin = parent->GetLastCommittedOrigin();
  }
  // TODO(csharrison): Replace the empty OnceClosure with a UI-triggering
  // callback.
  async_filter_ = base::MakeUnique<AsyncDocumentSubresourceFilter>(
      ruleset_handle_, std::move(params),
      base::Bind(&ActivationStateComputingNavigationThrottle::
                     SetActivationStateAndResume,
                 weak_ptr_factory_.GetWeakPtr()),
      base::OnceClosure());
  return content::NavigationThrottle::ThrottleCheckResult::DEFER;
}

void ActivationStateComputingNavigationThrottle::SetActivationStateAndResume(
    ActivationState state) {
  // Cannot send activation level to the renderer until ReadyToCommitNavigation,
  // the driver will pull the state out of |this| when that callback occurs.
  DCHECK(!activation_state_);
  activation_state_.emplace(state);
  navigation_handle()->Resume();
}

std::unique_ptr<AsyncDocumentSubresourceFilter>
ActivationStateComputingNavigationThrottle::ReleaseFilter() {
  return std::move(async_filter_);
}

const ActivationState&
ActivationStateComputingNavigationThrottle::GetActivationState() const {
  return activation_state_.value();
}

}  // namespace subresource_filter
