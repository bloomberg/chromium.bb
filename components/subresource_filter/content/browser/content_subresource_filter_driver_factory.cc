// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"

#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    subresource_filter::ContentSubresourceFilterDriverFactory);

namespace subresource_filter {

namespace {

// Returns true with a probability given by |performance_measurement_rate| if
// ThreadTicks is supported, otherwise returns false.
bool ShouldMeasurePerformanceForPageLoad(double performance_measurement_rate) {
  if (!base::ThreadTicks::IsSupported())
    return false;
  return performance_measurement_rate == 1 ||
         (performance_measurement_rate > 0 &&
          base::RandDouble() < performance_measurement_rate);
}

}  // namespace

// static
void ContentSubresourceFilterDriverFactory::CreateForWebContents(
    content::WebContents* web_contents,
    SubresourceFilterClient* client) {
  if (FromWebContents(web_contents))
    return;
  web_contents->SetUserData(
      UserDataKey(), base::MakeUnique<ContentSubresourceFilterDriverFactory>(
                         web_contents, client));
}

// static
ContentSubresourceFilterDriverFactory::ContentSubresourceFilterDriverFactory(
    content::WebContents* web_contents,
    SubresourceFilterClient* client)
    : content::WebContentsObserver(web_contents),
      client_(client),
      throttle_manager_(
          base::MakeUnique<ContentSubresourceFilterThrottleManager>(
              this,
              client_->GetRulesetDealer(),
              web_contents)) {}

ContentSubresourceFilterDriverFactory::
    ~ContentSubresourceFilterDriverFactory() {}

void ContentSubresourceFilterDriverFactory::NotifyPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    ActivationDecision activation_decision,
    Configuration::ActivationOptions matched_options) {
  DCHECK(navigation_handle->IsInMainFrame());
  DCHECK(!navigation_handle->IsSameDocument());
  if (navigation_handle->GetNetErrorCode() != net::OK)
    return;

  activation_decision_ = activation_decision;
  activation_options_ = matched_options;
  DCHECK_NE(activation_decision_, ActivationDecision::UNKNOWN);

  // ACTIVATION_DISABLED implies DISABLED activation level.
  DCHECK(activation_decision_ != ActivationDecision::ACTIVATION_DISABLED ||
         activation_options_.activation_level == ActivationLevel::DISABLED);
  ActivationState state = ActivationState(activation_options_.activation_level);
  state.measure_performance = ShouldMeasurePerformanceForPageLoad(
      activation_options_.performance_measurement_rate);

  // TODO(csharrison): Also use metadata returned from the safe browsing filter,
  // when it is available to set enable_logging. Add tests for this behavior.
  state.enable_logging =
      activation_options_.activation_level == ActivationLevel::ENABLED &&
      !activation_options_.should_suppress_notifications &&
      base::FeatureList::IsEnabled(
          kSafeBrowsingSubresourceFilterExperimentalUI);

  SubresourceFilterObserverManager::FromWebContents(web_contents())
      ->NotifyPageActivationComputed(navigation_handle, activation_decision_,
                                     state);
}

void ContentSubresourceFilterDriverFactory::OnFirstSubresourceLoadDisallowed() {
  if (activation_options_.should_suppress_notifications)
    return;
  client_->ToggleNotificationVisibility(activation_options_.activation_level ==
                                        ActivationLevel::ENABLED);
}

bool ContentSubresourceFilterDriverFactory::AllowStrongPopupBlocking() {
  return activation_options_.should_strengthen_popup_blocker;
}

void ContentSubresourceFilterDriverFactory::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    activation_decision_ = ActivationDecision::UNKNOWN;
    client_->ToggleNotificationVisibility(false);
  }
}

void ContentSubresourceFilterDriverFactory::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument() &&
      activation_decision_ == ActivationDecision::UNKNOWN &&
      navigation_handle->HasCommitted()) {
    activation_decision_ = ActivationDecision::ACTIVATION_DISABLED;
    activation_options_ = Configuration::ActivationOptions();
  }
}

}  // namespace subresource_filter
