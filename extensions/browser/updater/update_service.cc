// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_service.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/updater/extension_update_data.h"
#include "extensions/browser/updater/update_data_provider.h"
#include "extensions/browser/updater/update_service_factory.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_url_handlers.h"

namespace extensions {

namespace {

void SendUninstallPingCompleteCallback(update_client::Error error) {}

}  // namespace

// static
UpdateService* UpdateService::Get(content::BrowserContext* context) {
  return UpdateServiceFactory::GetForBrowserContext(context);
}

void UpdateService::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (update_data_provider_) {
    update_data_provider_->Shutdown();
    update_data_provider_ = nullptr;
  }
  update_client_->RemoveObserver(this);
  update_client_ = nullptr;
  browser_context_ = nullptr;
}

void UpdateService::SendUninstallPing(const std::string& id,
                                      const base::Version& version,
                                      int reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(update_client_);
  update_client_->SendUninstallPing(
      id, version, reason, base::BindOnce(&SendUninstallPingCompleteCallback));
}

bool UpdateService::CanUpdate(const std::string& extension_id) const {
  if (!base::FeatureList::IsEnabled(features::kNewExtensionUpdaterService))
    return false;
  // We can only update extensions that have been installed on the system.
  // Furthermore, we can only update extensions that were installed from the
  // webstore.
  const ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const Extension* extension = registry->GetInstalledExtension(extension_id);
  return extension && ManifestURL::UpdatesFromGallery(extension);
}

void UpdateService::OnEvent(Events event, const std::string& extension_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << "UpdateService::OnEvent " << static_cast<int>(event) << " "
          << extension_id;

  // When no update is found, a previous update check might have queued an
  // update for this extension because it was in use at the time. We should
  // ask for the install of the queued update now if it's ready.
  if (event == Events::COMPONENT_NOT_UPDATED) {
    ExtensionSystem::Get(browser_context_)
        ->FinishDelayedInstallationIfReady(extension_id,
                                           true /*install_immediately*/);
  }
}

UpdateService::UpdateService(
    content::BrowserContext* browser_context,
    scoped_refptr<update_client::UpdateClient> update_client)
    : browser_context_(browser_context),
      update_client_(update_client),
      weak_ptr_factory_(this) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(update_client_);
  update_data_provider_ =
      base::MakeRefCounted<UpdateDataProvider>(browser_context_);
  update_client->AddObserver(this);
}

UpdateService::~UpdateService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (update_client_)
    update_client_->RemoveObserver(this);
}

void UpdateService::StartUpdateCheck(
    const ExtensionUpdateCheckParams& update_params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << "UpdateService::StartUpdateCheck";

  if (!ExtensionsBrowserClient::Get()->IsBackgroundUpdateAllowed()) {
    VLOG(1) << "UpdateService - Extension update not allowed.";
    return;
  }

  ExtensionUpdateDataMap update_data;
  std::vector<std::string> extension_ids;
  for (const auto& update_info : update_params.update_info) {
    const std::string& extension_id = update_info.first;
    if (updating_extensions_.find(extension_id) != updating_extensions_.end())
      continue;

    updating_extensions_.insert(extension_id);
    extension_ids.push_back(extension_id);

    ExtensionUpdateData data = update_info.second;
    if (data.install_source.empty() &&
        update_params.priority == ExtensionUpdateCheckParams::FOREGROUND) {
      data.install_source = "ondemand";
    }
    update_data.insert(std::make_pair(extension_id, data));
  }
  if (extension_ids.empty())
    return;

  update_client_->Update(
      extension_ids,
      base::BindOnce(&UpdateDataProvider::GetData, update_data_provider_,
                     std::move(update_data)),
      false,
      base::BindOnce(&UpdateService::UpdateCheckComplete,
                     weak_ptr_factory_.GetWeakPtr(), extension_ids));
}

void UpdateService::UpdateCheckComplete(
    const std::vector<std::string>& extension_ids,
    update_client::Error error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << "UpdateService::UpdateCheckComplete";
  for (const std::string& extension_id : extension_ids)
    updating_extensions_.erase(extension_id);
}

}  // namespace extensions
