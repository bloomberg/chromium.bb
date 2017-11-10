// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/interventions_internals/interventions_internals_page_handler.h"

#include <unordered_map>

#include "components/previews/core/previews_experiments.h"

namespace {

// Key for status mapping.
const char kAmpRedirectionPreviews[] = "ampPreviews";
const char kClientLoFiPreviews[] = "clientLoFiPreviews";
const char kNoScriptPreviews[] = "noScriptPreviews";
const char kOfflinePreviews[] = "offlinePreviews";

// Description for statuses.
const char kAmpRedirectionDescription[] = "AMP Previews";
const char kClientLoFiDescription[] = "Client LoFi Previews";
const char kNoScriptDescription[] = "NoScript Previews";
const char kOfflineDesciption[] = "Offline Previews";

}  // namespace

InterventionsInternalsPageHandler::InterventionsInternalsPageHandler(
    mojom::InterventionsInternalsPageHandlerRequest request,
    previews::PreviewsUIService* previews_ui_service)
    : binding_(this, std::move(request)),
      previews_ui_service_(previews_ui_service),
      current_estimated_ect_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
  logger_ = previews_ui_service_->previews_logger();
  DCHECK(logger_);
}

InterventionsInternalsPageHandler::~InterventionsInternalsPageHandler() {
  DCHECK(logger_);
  logger_->RemoveObserver(this);
}

void InterventionsInternalsPageHandler::SetClientPage(
    mojom::InterventionsInternalsPagePtr page) {
  page_ = std::move(page);
  DCHECK(page_);
  OnEffectiveConnectionTypeChanged(current_estimated_ect_);
  logger_->AddAndNotifyObserver(this);
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
