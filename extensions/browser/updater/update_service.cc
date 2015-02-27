// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_service.h"

#include <set>

#include "base/message_loop/message_loop.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/update_service_factory.h"
#include "extensions/common/extension_urls.h"

using update_client::UpdateQueryParams;

namespace extensions {

// static
UpdateService* UpdateService::Get(content::BrowserContext* context) {
  return UpdateServiceFactory::GetForBrowserContext(context);
}

void UpdateService::DownloadAndInstall(
    const std::string& id,
    const base::Callback<void(bool)>& callback) {
  DCHECK(download_callback_.is_null());
  download_callback_ = callback;
  downloader_->AddPendingExtension(id, extension_urls::GetWebstoreUpdateUrl(),
                                   0);
  downloader_->StartAllPending(nullptr);
}

UpdateService::UpdateService(content::BrowserContext* context)
    : browser_context_(context),
      downloader_(new ExtensionDownloader(this, context->GetRequestContext())) {
  downloader_->set_manifest_query_params(
      UpdateQueryParams::Get(UpdateQueryParams::CRX));
}

UpdateService::~UpdateService() {
}

void UpdateService::OnExtensionDownloadFailed(
    const std::string& id,
    Error error,
    const PingResult& ping,
    const std::set<int>& request_ids) {
  auto callback = download_callback_;
  download_callback_.Reset();
  callback.Run(false);
}

void UpdateService::OnExtensionDownloadFinished(
    const CRXFileInfo& file,
    bool file_ownership_passed,
    const GURL& download_url,
    const std::string& version,
    const PingResult& ping,
    const std::set<int>& request_id,
    const InstallCallback& install_callback) {
  // TODO(rockot): Actually unpack and install the CRX.
  auto callback = download_callback_;
  download_callback_.Reset();
  callback.Run(true);
  if (!install_callback.is_null())
    install_callback.Run(true);
}

bool UpdateService::IsExtensionPending(const std::string& id) {
  // TODO(rockot): Implement this. For now all IDs are "pending".
  return true;
}

bool UpdateService::GetExtensionExistingVersion(const std::string& id,
                                                std::string* version) {
  // TODO(rockot): Implement this.
  return false;
}

}  // namespace extensions
