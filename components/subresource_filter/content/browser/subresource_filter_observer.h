// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_H_

#include "components/subresource_filter/core/common/activation_decision.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace subresource_filter {

struct ActivationState;

// Class to receive notifications of subresource filter events for a given
// WebContents. Registered with a SubresourceFilterObserverManager.
class SubresourceFilterObserver {
 public:
  virtual ~SubresourceFilterObserver() = default;

  virtual void OnSubresourceFilterGoingAway() {}

  virtual void OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      ActivationDecision activation_decision,
      const ActivationState& activation_state) {}
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_H_
