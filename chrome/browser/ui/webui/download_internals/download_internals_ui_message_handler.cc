// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/download_internals/download_internals_ui_message_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/download_service.h"
#include "content/public/browser/web_ui.h"

namespace download_internals {

DownloadInternalsUIMessageHandler::DownloadInternalsUIMessageHandler()
    : download_service_(nullptr), weak_ptr_factory_(this) {}

DownloadInternalsUIMessageHandler::~DownloadInternalsUIMessageHandler() {
  if (download_service_)
    download_service_->GetLogger()->RemoveObserver(this);
}

void DownloadInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getServiceStatus",
      base::Bind(&DownloadInternalsUIMessageHandler::HandleGetServiceStatus,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getServiceDownloads",
      base::Bind(&DownloadInternalsUIMessageHandler::HandleGetServiceDownloads,
                 weak_ptr_factory_.GetWeakPtr()));

  Profile* profile = Profile::FromWebUI(web_ui());
  download_service_ = DownloadServiceFactory::GetForBrowserContext(profile);
  download_service_->GetLogger()->AddObserver(this);
}

void DownloadInternalsUIMessageHandler::OnServiceStatusChanged(
    const base::Value& service_status) {
  if (!IsJavascriptAllowed())
    return;

  FireWebUIListener("service-status-changed", service_status);
}

void DownloadInternalsUIMessageHandler::OnServiceDownloadsAvailable(
    const base::Value& service_downloads) {
  if (!IsJavascriptAllowed())
    return;

  FireWebUIListener("service-downloads-available", service_downloads);
}

void DownloadInternalsUIMessageHandler::OnServiceDownloadChanged(
    const base::Value& service_download) {
  if (!IsJavascriptAllowed())
    return;

  FireWebUIListener("service-download-changed", service_download);
}

void DownloadInternalsUIMessageHandler::OnServiceDownloadFailed(
    const base::Value& service_download) {
  if (!IsJavascriptAllowed())
    return;

  FireWebUIListener("service-download-failed", service_download);
}

void DownloadInternalsUIMessageHandler::OnServiceRequestMade(
    const base::Value& service_request) {
  if (!IsJavascriptAllowed())
    return;

  FireWebUIListener("service-request-made", service_request);
}

void DownloadInternalsUIMessageHandler::HandleGetServiceStatus(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  ResolveJavascriptCallback(*callback_id,
                            download_service_->GetLogger()->GetServiceStatus());
}

void DownloadInternalsUIMessageHandler::HandleGetServiceDownloads(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  ResolveJavascriptCallback(
      *callback_id, download_service_->GetLogger()->GetServiceDownloads());
}

}  // namespace download_internals
