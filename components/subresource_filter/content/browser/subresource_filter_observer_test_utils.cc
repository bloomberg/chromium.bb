// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"

#include "content/public/browser/navigation_handle.h"

namespace subresource_filter {

TestSubresourceFilterObserver::TestSubresourceFilterObserver(
    content::WebContents* web_contents)
    : scoped_observer_(this) {
  scoped_observer_.Add(
      SubresourceFilterObserverManager::FromWebContents(web_contents));
}

TestSubresourceFilterObserver::~TestSubresourceFilterObserver() {}

void TestSubresourceFilterObserver::OnSubresourceFilterGoingAway() {
  scoped_observer_.RemoveAll();
}

void TestSubresourceFilterObserver::OnPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    ActivationDecision activation_decision,
    const ActivationState& activation_state) {
  page_activations_[navigation_handle->GetURL()] = activation_decision;
}

void TestSubresourceFilterObserver::OnSubframeNavigationEvaluated(
    content::NavigationHandle* navigation_handle,
    LoadPolicy load_policy) {
  subframe_load_evaluations_[navigation_handle->GetURL()] = load_policy;
}

base::Optional<ActivationDecision>
TestSubresourceFilterObserver::GetPageActivation(const GURL& url) {
  auto it = page_activations_.find(url);
  if (it != page_activations_.end())
    return it->second;
  return base::Optional<ActivationDecision>();
}

base::Optional<LoadPolicy> TestSubresourceFilterObserver::GetSubframeLoadPolicy(
    const GURL& url) {
  auto it = subframe_load_evaluations_.find(url);
  if (it != subframe_load_evaluations_.end())
    return it->second;
  return base::Optional<LoadPolicy>();
}

}  // namespace subresource_filter
