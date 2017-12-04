// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_data_provider.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/post_task.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"

namespace extensions {

UpdateDataProvider::UpdateDataProvider(content::BrowserContext* context,
                                       InstallCallback install_callback)
    : context_(context), install_callback_(std::move(install_callback)) {}

UpdateDataProvider::~UpdateDataProvider() {}

void UpdateDataProvider::Shutdown() {
  context_ = nullptr;
}

void UpdateDataProvider::GetData(
    const std::vector<std::string>& ids,
    std::vector<update_client::CrxComponent>* data) {
  if (!context_)
    return;
  const ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
  const ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(context_);
  for (const auto& id : ids) {
    const Extension* extension = registry->GetInstalledExtension(id);
    if (!extension)
      continue;
    data->push_back(update_client::CrxComponent());
    update_client::CrxComponent* info = &data->back();
    std::string pubkey_bytes;
    base::Base64Decode(extension->public_key(), &pubkey_bytes);
    info->pk_hash.resize(crypto::kSHA256Length, 0);
    crypto::SHA256HashString(pubkey_bytes, info->pk_hash.data(),
                             info->pk_hash.size());
    info->version = *extension->version();
    info->allows_background_download = false;
    info->requires_network_encryption = true;
    info->installer = base::MakeRefCounted<ExtensionInstaller>(
        id, extension->path(),
        base::BindOnce(&UpdateDataProvider::RunInstallCallback, this));
    if (!ExtensionsBrowserClient::Get()->IsExtensionEnabled(id, context_)) {
      int disabled_reasons = extension_prefs->GetDisableReasons(id);
      if (disabled_reasons == extensions::disable_reason::DISABLE_NONE ||
          disabled_reasons >= extensions::disable_reason::DISABLE_REASON_LAST) {
        info->disabled_reasons.push_back(0);
      }
      for (int enum_value = 1;
           enum_value < extensions::disable_reason::DISABLE_REASON_LAST;
           enum_value <<= 1) {
        if (disabled_reasons & enum_value)
          info->disabled_reasons.push_back(enum_value);
      }
    }
  }
}

void UpdateDataProvider::RunInstallCallback(
    const std::string& extension_id,
    const std::string& public_key,
    const base::FilePath& unpacked_dir,
    UpdateClientCallback update_client_callback) {
  if (!context_) {
    base::PostTaskWithTraits(
        FROM_HERE, {base::TaskPriority::BACKGROUND, base::MayBlock()},
        base::BindOnce(base::IgnoreResult(&base::DeleteFile), unpacked_dir,
                       true));
    return;
  }

  DCHECK(!install_callback_.is_null());
  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
      ->PostTask(FROM_HERE,
                 base::BindOnce(std::move(install_callback_), context_,
                                extension_id, public_key, unpacked_dir,
                                std::move(update_client_callback)));
}

}  // namespace extensions
