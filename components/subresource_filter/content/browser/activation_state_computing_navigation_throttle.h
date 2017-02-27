// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_FRAME_ACTIVATION_NAVIGATION_THROTTLE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_FRAME_ACTIVATION_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/subresource_filter/content/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "content/public/browser/navigation_throttle.h"

namespace subresource_filter {

class AsyncDocumentSubresourceFilter;

// NavigationThrottle responsible for determining the activation state of
// subresource filtering for a given navigation (either in the main frame or in
// a subframe); and for deferring that navigation at WillProcessResponse until
// the activation state computation on the blocking pool thread is complete.
// Interested parties can retrieve the activation state after this point (most
// likely in ReadyToCommitNavigation).
class ActivationStateComputingNavigationThrottle
    : public content::NavigationThrottle {
 public:
  // For main frames, a verified ruleset handle is not readily available at
  // construction time. Since it is expensive to "warm up" the ruleset, the
  // ruleset handle will be injected in NotifyPageActivationWithRuleset once it
  // has been established that activation computation is needed.
  static std::unique_ptr<ActivationStateComputingNavigationThrottle>
  CreateForMainFrame(content::NavigationHandle* navigation_handle);

  // It is illegal to create an activation computing throttle for subframes
  // whose parents are not activated. Similarly, |ruleset_handle| should be
  // non-null.
  static std::unique_ptr<ActivationStateComputingNavigationThrottle>
  CreateForSubframe(content::NavigationHandle* navigation_handle,
                    VerifiedRuleset::Handle* ruleset_handle,
                    const ActivationState& parent_activation_state);

  ~ActivationStateComputingNavigationThrottle() override;

  // Notification for main frames when the page level activation is computed.
  // Must be called at most once before WillProcessResponse is called on this
  // throttle. If it is never called, or it is called with a DISABLED state,
  // this object will never delay the navigation.
  void NotifyPageActivationWithRuleset(
      VerifiedRuleset::Handle* ruleset_handle,
      const ActivationState& page_activation_state);

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;

  // After the navigation is finished, the client may optionally choose to
  // continue using the DocumentSubresourceFilter that was used to compute the
  // activation state for this frame. The transfered filter can be cached and
  // used to calculate load policy for subframe navigations occuring in this
  // frame.
  std::unique_ptr<AsyncDocumentSubresourceFilter> ReleaseFilter();

  // Gets the activation state calculated for this navigation. Must be called
  // after the navigation is resumed from getting paused in WillProcessResponse,
  // which, for example, will have happened at ReadyToCommitNavigation.
  const ActivationState& GetActivationState() const;

 private:
  void SetActivationStateAndResume(ActivationState state);

  ActivationStateComputingNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      const base::Optional<ActivationState> parent_activation_state,
      VerifiedRuleset::Handle* ruleset_handle);

  // These members are optional to allow DCHECKing their existence at certain
  // points in the navigation flow.
  base::Optional<ActivationState> activation_state_;
  base::Optional<ActivationState> parent_activation_state_;

  std::unique_ptr<AsyncDocumentSubresourceFilter> async_filter_;

  // Must outlive this class. For main frame navigations, this member will be
  // nullptr until NotifyPageActivationWithRuleset is called.
  VerifiedRuleset::Handle* ruleset_handle_;

  base::WeakPtrFactory<ActivationStateComputingNavigationThrottle>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ActivationStateComputingNavigationThrottle);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_FRAME_ACTIVATION_NAVIGATION_THROTTLE_H_
