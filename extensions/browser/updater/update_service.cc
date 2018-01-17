// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_service.h"

#include <map>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/updater/extension_update_data.h"
#include "extensions/browser/updater/update_data_provider.h"
#include "extensions/browser/updater/update_service_factory.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_url_handlers.h"

namespace {

void SendUninstallPingCompleteCallback(update_client::Error error) {}

}  // namespace

namespace extensions {

// static
UpdateService* UpdateService::Get(content::BrowserContext* context) {
  return UpdateServiceFactory::GetForBrowserContext(context);
}

void UpdateService::Shutdown() {
  if (update_data_provider_) {
    update_data_provider_->Shutdown();
    update_data_provider_ = nullptr;
  }
  update_client_ = nullptr;
  browser_context_ = nullptr;
}

void UpdateService::SendUninstallPing(const std::string& id,
                                      const base::Version& version,
                                      int reason) {
  DCHECK(update_client_);
  update_client_->SendUninstallPing(
      id, version, reason, base::BindOnce(&SendUninstallPingCompleteCallback));
}

bool UpdateService::CanUpdate(const std::string& extension_id) const {
  // We can only update extensions that have been installed on the system.
  // Furthermore, we can only update extensions that were installed from the
  // webstore.
  const ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const Extension* extension = registry->GetInstalledExtension(extension_id);
  return extension && ManifestURL::UpdatesFromGallery(extension);
}

UpdateService::UpdateService(
    content::BrowserContext* browser_context,
    scoped_refptr<update_client::UpdateClient> update_client)
    : browser_context_(browser_context),
      update_client_(update_client),
      weak_ptr_factory_(this) {
  DCHECK(update_client_);
  update_data_provider_ =
      base::MakeRefCounted<UpdateDataProvider>(browser_context_);
}

UpdateService::~UpdateService() {}

void UpdateService::StartUpdateCheck(
    const ExtensionUpdateCheckParams& update_params) {
  ExtensionUpdateDataMap update_data;
  std::vector<std::string> extension_ids;
  for (const auto& info : update_params.update_info) {
    const std::string& extension_id = info.first;
    if (updating_extensions_.find(extension_id) != updating_extensions_.end())
      continue;

    updating_extensions_.insert(extension_id);
    extension_ids.push_back(extension_id);
    update_data[extension_id] = info.second;
  }
  if (extension_ids.empty())
    return;
  if (update_params.priority == ExtensionUpdateCheckParams::BACKGROUND) {
    update_client_->Update(
        extension_ids,
        base::BindOnce(&UpdateDataProvider::GetData, update_data_provider_,
                       std::move(update_data)),
        base::BindOnce(&UpdateService::UpdateCheckComplete,
                       weak_ptr_factory_.GetWeakPtr(), extension_ids));
  } else {
    // TODO (mxnguyen): Run the on demand update in batch.
    for (const std::string& extension_id : extension_ids) {
      ExtensionUpdateDataMap update_data_for_extension;
      update_data_for_extension[extension_id] =
          std::move(update_data[extension_id]);
      update_client_->Install(
          extension_id,
          base::BindOnce(&UpdateDataProvider::GetData, update_data_provider_,
                         std::move(update_data_for_extension)),
          base::BindOnce(&UpdateService::UpdateCheckComplete,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::vector<std::string>({extension_id})));
    }
  }
}

void UpdateService::UpdateCheckComplete(
    const std::vector<std::string>& extension_ids,
    update_client::Error error) {
  for (const std::string& extension_id : extension_ids)
    updating_extensions_.erase(extension_id);
}

}  // namespace extensions
