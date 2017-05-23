// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"

#include "components/subresource_filter/core/common/activation_state.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    subresource_filter::SubresourceFilterObserverManager);

namespace subresource_filter {

SubresourceFilterObserverManager::SubresourceFilterObserverManager(
    content::WebContents* web_contents) {}

SubresourceFilterObserverManager::~SubresourceFilterObserverManager() {
  for (auto& observer : observers_)
    observer.OnSubresourceFilterGoingAway();
}

void SubresourceFilterObserverManager::AddObserver(
    SubresourceFilterObserver* observer) {
  observers_.AddObserver(observer);
}

void SubresourceFilterObserverManager::RemoveObserver(
    SubresourceFilterObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SubresourceFilterObserverManager::NotifyPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    ActivationDecision activation_decision,
    const ActivationState& activation_state) {
  for (auto& observer : observers_) {
    observer.OnPageActivationComputed(navigation_handle, activation_decision,
                                      activation_state);
  }
}

}  // namespace subresource_filter
