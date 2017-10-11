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
const char kOfflinePreviews[] = "offlinePreviews";

// Description for statuses.
const char kAmpRedirectionDescription[] = "AMP Previews";
const char kClientLoFiDescription[] = "Client LoFi Previews";
const char kOfflineDesciption[] = "Offline Previews";

}  // namespace

InterventionsInternalsPageHandler::InterventionsInternalsPageHandler(
    mojom::InterventionsInternalsPageHandlerRequest request,
    previews::PreviewsLogger* logger)
    : binding_(this, std::move(request)), logger_(logger) {
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
  logger_->AddAndNotifyObserver(this);
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

  auto offline_status = mojom::PreviewsStatus::New();
  offline_status->description = kOfflineDesciption;
  offline_status->enabled = previews::params::IsOfflinePreviewsEnabled();
  statuses[kOfflinePreviews] = std::move(offline_status);

  std::move(callback).Run(std::move(statuses));
}
