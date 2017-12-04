// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_service.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/updater/update_data_provider.h"
#include "extensions/browser/updater/update_service_factory.h"

namespace {

using UpdateClientCallback =
    extensions::UpdateDataProvider::UpdateClientCallback;

void UpdateCheckCompleteCallback(update_client::Error error) {}

void SendUninstallPingCompleteCallback(update_client::Error error) {}

void InstallUpdateCallback(content::BrowserContext* context,
                           const std::string& extension_id,
                           const std::string& public_key,
                           const base::FilePath& unpacked_dir,
                           UpdateClientCallback update_client_callback) {
  using InstallError = update_client::InstallError;
  using Result = update_client::CrxInstaller::Result;
  extensions::ExtensionSystem::Get(context)->InstallUpdate(
      extension_id, public_key, unpacked_dir,
      base::BindOnce(
          [](UpdateClientCallback update_client_callback, bool success) {
            DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
            std::move(update_client_callback)
                .Run(Result(success ? InstallError::NONE
                                    : InstallError::GENERIC_ERROR));
          },
          std::move(update_client_callback)));
}

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
  context_ = nullptr;
}

void UpdateService::SendUninstallPing(const std::string& id,
                                      const base::Version& version,
                                      int reason) {
  update_client_->SendUninstallPing(
      id, version, reason, base::BindOnce(&SendUninstallPingCompleteCallback));
}

void UpdateService::StartUpdateCheck(
    const std::vector<std::string>& extension_ids) {
  if (!update_client_)
    return;
  update_client_->Update(
      extension_ids,
      base::BindOnce(&UpdateDataProvider::GetData, update_data_provider_),
      base::BindOnce(&UpdateCheckCompleteCallback));
}

UpdateService::UpdateService(
    content::BrowserContext* context,
    scoped_refptr<update_client::UpdateClient> update_client)
    : context_(context), update_client_(update_client) {
  CHECK(update_client_);
  update_data_provider_ = base::MakeRefCounted<UpdateDataProvider>(
      context_, base::BindOnce(&InstallUpdateCallback));
}

UpdateService::~UpdateService() {}

}  // namespace extensions
