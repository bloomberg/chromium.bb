// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/interventions_internals/interventions_internals_page_handler.h"

#include <unordered_map>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service.h"
#include "chrome/common/chrome_switches.h"
#include "components/previews/core/previews_experiments.h"

namespace {

// Keys for status mapping. These value will be used as HTML DOM ID in the
// JavaScript code.
const char kAmpRedirectionPreviews[] = "ampPreviews";
const char kClientLoFiPreviews[] = "clientLoFiPreviews";
const char kNoScriptPreviews[] = "noScriptPreviews";
const char kOfflinePreviews[] = "offlinePreviews";

// Descriptions for previews.
const char kAmpRedirectionDescription[] = "AMP Previews";
const char kClientLoFiDescription[] = "Client LoFi Previews";
const char kNoScriptDescription[] = "NoScript Previews";
const char kOfflineDesciption[] = "Offline Previews";

// Flag feature name.
const char kNoScriptFeatureName[] = "NoScriptPreviews";
#if defined(OS_ANDROID)
const char kOfflinePageFeatureName[] = "OfflinePreviews";
#endif  // OS_ANDROID

// Keys for flags mapping.
const char kEctFlag[] = "ectFlag";
const char kNoScriptFlag[] = "noScriptFlag";
const char kOfflinePageFlag[] = "offlinePageFlag";

// Links to flags in chrome://flags.
// TODO(thanhdle): Refactor into vector of structs. crbug.com/787010.
const char kEctFlagLink[] = "chrome://flags/#force-effective-connection-type";
const char kNoScriptFlagLink[] = "chrome://flags/#enable-noscript-previews";
const char kOfflinePageFlagLink[] = "chrome://flags/#enable-offline-previews";

const char kDefaultFlagValue[] = "Default";

// Check if the flag status of the flag is a forced value or not.
std::string GetFeatureFlagStatus(const std::string& feature_name) {
  std::string enabled_features =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kEnableFeatures);
  if (enabled_features.find(feature_name) != std::string::npos) {
    return "Enabled";
  }
  std::string disabled_features =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDisableFeatures);
  if (disabled_features.find(feature_name) != std::string::npos) {
    return "Disabled";
  }
  return kDefaultFlagValue;
}

}  // namespace

InterventionsInternalsPageHandler::InterventionsInternalsPageHandler(
    mojom::InterventionsInternalsPageHandlerRequest request,
    previews::PreviewsUIService* previews_ui_service,
    UINetworkQualityEstimatorService* ui_nqe_service)
    : binding_(this, std::move(request)),
      previews_ui_service_(previews_ui_service),
      ui_nqe_service_(ui_nqe_service),
      current_estimated_ect_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
  logger_ = previews_ui_service_->previews_logger();
  DCHECK(logger_);
}

InterventionsInternalsPageHandler::~InterventionsInternalsPageHandler() {
  DCHECK(logger_);
  logger_->RemoveObserver(this);
  ui_nqe_service_->RemoveEffectiveConnectionTypeObserver(this);
}

void InterventionsInternalsPageHandler::SetClientPage(
    mojom::InterventionsInternalsPagePtr page) {
  page_ = std::move(page);
  DCHECK(page_);
  OnEffectiveConnectionTypeChanged(current_estimated_ect_);
  logger_->AddAndNotifyObserver(this);
  ui_nqe_service_->AddEffectiveConnectionTypeObserver(this);
}

void InterventionsInternalsPageHandler::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType type) {
  current_estimated_ect_ = type;
  if (!page_) {
    // Don't try to notify the page if |page_| is not ready.
    return;
  }
  page_->OnEffectiveConnectionTypeChanged(
      net::GetNameForEffectiveConnectionType(type));
}

void InterventionsInternalsPageHandler::OnNewMessageLogAdded(
    const previews::PreviewsLogger::MessageLog& message) {
  mojom::MessageLogPtr mojo_message_ptr(mojom::MessageLog::New());

  mojo_message_ptr->type = message.event_type;
  mojo_message_ptr->description = message.event_description;
  mojo_message_ptr->url = message.url;
  mojo_message_ptr->time = message.time.ToJavaTime();
  mojo_message_ptr->page_id = message.page_id;

  page_->LogNewMessage(std::move(mojo_message_ptr));
}

void InterventionsInternalsPageHandler::SetIgnorePreviewsBlacklistDecision(
    bool ignored) {
  previews_ui_service_->SetIgnorePreviewsBlacklistDecision(ignored);
}

void InterventionsInternalsPageHandler::OnLastObserverRemove() {
  // Reset the status of ignoring PreviewsBlackList decisions to false.
  previews_ui_service_->SetIgnorePreviewsBlacklistDecision(false /* ignored */);
}

void InterventionsInternalsPageHandler::OnIgnoreBlacklistDecisionStatusChanged(
    bool ignored) {
  page_->OnIgnoreBlacklistDecisionStatusChanged(ignored);
}

void InterventionsInternalsPageHandler::OnNewBlacklistedHost(
    const std::string& host,
    base::Time time) {
  page_->OnBlacklistedHost(host, time.ToJavaTime());
}

void InterventionsInternalsPageHandler::OnUserBlacklistedStatusChange(
    bool blacklisted) {
  page_->OnUserBlacklistedStatusChange(blacklisted);
}

void InterventionsInternalsPageHandler::OnBlacklistCleared(base::Time time) {
  page_->OnBlacklistCleared(time.ToJavaTime());
}

void InterventionsInternalsPageHandler::GetPreviewsEnabled(
    GetPreviewsEnabledCallback callback) {
  std::unordered_map<std::string, mojom::PreviewsStatusPtr> statuses;

  auto amp_status = mojom::PreviewsStatus::New();
  amp_status->description = kAmpRedirectionDescription;
  amp_status->enabled = previews::params::IsAMPRedirectionPreviewEnabled();
  statuses[kAmpRedirectionPreviews] = std::move(amp_status);

  auto client_lofi_status = mojom::PreviewsStatus::New();
  client_lofi_status->description = kClientLoFiDescription;
  client_lofi_status->enabled = previews::params::IsClientLoFiEnabled();
  statuses[kClientLoFiPreviews] = std::move(client_lofi_status);

  auto noscript_status = mojom::PreviewsStatus::New();
  noscript_status->description = kNoScriptDescription;
  noscript_status->enabled = previews::params::IsNoScriptPreviewsEnabled();
  statuses[kNoScriptPreviews] = std::move(noscript_status);

  auto offline_status = mojom::PreviewsStatus::New();
  offline_status->description = kOfflineDesciption;
  offline_status->enabled = previews::params::IsOfflinePreviewsEnabled();
  statuses[kOfflinePreviews] = std::move(offline_status);

  std::move(callback).Run(std::move(statuses));
}

void InterventionsInternalsPageHandler::GetPreviewsFlagsDetails(
    GetPreviewsFlagsDetailsCallback callback) {
  std::unordered_map<std::string, mojom::PreviewsFlagPtr> flags;

  auto ect_status = mojom::PreviewsFlag::New();
  ect_status->description =
      flag_descriptions::kForceEffectiveConnectionTypeName;
  ect_status->link = kEctFlagLink;
  std::string ect_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kForceEffectiveConnectionType);
  ect_status->value = ect_value.empty() ? kDefaultFlagValue : ect_value;
  flags[kEctFlag] = std::move(ect_status);

  auto noscript_status = mojom::PreviewsFlag::New();
  noscript_status->description = flag_descriptions::kEnableNoScriptPreviewsName;
  noscript_status->link = kNoScriptFlagLink;
  noscript_status->value = GetFeatureFlagStatus(kNoScriptFeatureName);
  flags[kNoScriptFlag] = std::move(noscript_status);

  auto offline_page_status = mojom::PreviewsFlag::New();
#if defined(OS_ANDROID)
  offline_page_status->description =
      flag_descriptions::kEnableOfflinePreviewsName;
  offline_page_status->value = GetFeatureFlagStatus(kOfflinePageFeatureName);
#else
  offline_page_status->description = "Offline Page Previews";
  offline_page_status->value = "Only support on Android";
#endif  // OS_ANDROID
  offline_page_status->link = kOfflinePageFlagLink;
  flags[kOfflinePageFlag] = std::move(offline_page_status);

  std::move(callback).Run(std::move(flags));
}
