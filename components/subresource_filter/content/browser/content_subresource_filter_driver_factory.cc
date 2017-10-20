// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/browser/page_load_statistics.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/console_message_level.h"
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
    const Configuration& matched_configuration,
    bool warning) {
  DCHECK(navigation_handle->IsInMainFrame());
  DCHECK(!navigation_handle->IsSameDocument());
  if (navigation_handle->GetNetErrorCode() != net::OK)
    return;

  activation_decision_ = activation_decision;
  matched_configuration_ = matched_configuration;
  DCHECK_NE(activation_decision_, ActivationDecision::UNKNOWN);

  // ACTIVATION_DISABLED implies DISABLED activation level.
  DCHECK(activation_decision_ != ActivationDecision::ACTIVATION_DISABLED ||
         activation_options().activation_level == ActivationLevel::DISABLED);

  // Ensure the matched config is in our config list. If it wasn't then this
  // must be a forced activation via devtools.
  bool forced_activation_via_devtools =
      (matched_configuration == Configuration::MakeForForcedActivation());
  DCHECK(activation_decision_ != ActivationDecision::ACTIVATED ||
         HasEnabledConfiguration(matched_configuration) ||
         forced_activation_via_devtools)
      << matched_configuration;

  ActivationState state =
      ActivationState(activation_options().activation_level);
  state.measure_performance = ShouldMeasurePerformanceForPageLoad(
      activation_options().performance_measurement_rate);

  // This bit keeps track of BAS enforcement-style logging, not warning logging.
  state.enable_logging =
      activation_options().activation_level == ActivationLevel::ENABLED &&
      !activation_options().should_suppress_notifications &&
      matched_configuration != Configuration::MakeForForcedActivation() &&
      base::FeatureList::IsEnabled(
          kSafeBrowsingSubresourceFilterExperimentalUI);

  if (warning &&
      activation_options().activation_level == ActivationLevel::ENABLED) {
    DCHECK(on_commit_warning_messages_.empty());
    SetOnCommitWarningMessages();
    // Do not disallow enforcement if activated via devtools.
    if (!forced_activation_via_devtools) {
      activation_decision_ = ActivationDecision::ACTIVATION_DISABLED;
      state.activation_level = ActivationLevel::DISABLED;
      matched_configuration_.activation_options.activation_level =
          ActivationLevel::DISABLED;
    }
  }

  SubresourceFilterObserverManager::FromWebContents(web_contents())
      ->NotifyPageActivationComputed(navigation_handle, activation_decision_,
                                     state);
}

// Blocking popups here should trigger the standard popup blocking UI, so don't
// force the subresource filter specific UI.
bool ContentSubresourceFilterDriverFactory::ShouldDisallowNewWindow(
    const content::OpenURLParams* open_url_params) {
  if (activation_options().activation_level != ActivationLevel::ENABLED ||
      !activation_options().should_strengthen_popup_blocker) {
    return false;
  }

  // Block new windows from navigations whose triggering JS Event has an
  // isTrusted bit set to false. This bit is set to true if the event is
  // generated via a user action. See docs:
  // https://developer.mozilla.org/en-US/docs/Web/API/Event/isTrusted
  bool should_block = true;
  if (open_url_params) {
    should_block = open_url_params->triggering_event_info ==
                   blink::WebTriggeringEventInfo::kFromUntrustedEvent;
  }
  if (should_block) {
    web_contents()->GetMainFrame()->AddMessageToConsole(
        content::CONSOLE_MESSAGE_LEVEL_ERROR, kDisallowNewWindowMessage);
  }
  return should_block;
}

void ContentSubresourceFilterDriverFactory::OnFirstSubresourceLoadDisallowed() {
  if (activation_options().should_suppress_notifications)
    return;
  // This shouldn't happen normally, but in the rare case that an IPC from a
  // previous page arrives late we should guard against it.
  if (activation_options().should_disable_ruleset_rules ||
      activation_options().activation_level != ActivationLevel::ENABLED) {
    return;
  }
  client_->ShowNotification();
}

bool ContentSubresourceFilterDriverFactory::AllowRulesetRules() {
  return !activation_options().should_disable_ruleset_rules;
}

void ContentSubresourceFilterDriverFactory::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    activation_decision_ = ActivationDecision::UNKNOWN;
    client_->OnNewNavigationStarted();
  }
}

void ContentSubresourceFilterDriverFactory::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  std::vector<std::string> log_messages =
      std::move(on_commit_warning_messages_);

  if (!navigation_handle->HasCommitted())
    return;

  DCHECK(on_commit_warning_messages_.empty());

  if (activation_decision_ == ActivationDecision::UNKNOWN) {
    activation_decision_ = ActivationDecision::ACTIVATION_DISABLED;
    matched_configuration_ = Configuration();
    return;
  }

  content::RenderFrameHost* frame_host =
      navigation_handle->GetRenderFrameHost();
  for (auto& warning_message : log_messages) {
    frame_host->AddMessageToConsole(content::CONSOLE_MESSAGE_LEVEL_WARNING,
                                    warning_message);
  }
}

void ContentSubresourceFilterDriverFactory::SetOnCommitWarningMessages() {
  DCHECK_EQ(ActivationLevel::ENABLED, activation_options().activation_level);
  // If the matched configuration *would have* triggered resource blocking,
  // log a warning.
  if (!activation_options().should_disable_ruleset_rules &&
      !activation_options().should_suppress_notifications) {
    on_commit_warning_messages_.push_back(kActivationWarningConsoleMessage);
  }

  // If the matched configuration *would have* triggered new tab/window
  // blocking, log a warning.
  if (activation_options().should_strengthen_popup_blocker)
    on_commit_warning_messages_.push_back(kDisallowNewWindowWarningMessage);
}

}  // namespace subresource_filter
