// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/install_extension_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/chrome_zipfile_installer.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/feature_switch.h"
#include "net/base/filename_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

InstallExtensionHandler::InstallExtensionHandler() {
}

InstallExtensionHandler::~InstallExtensionHandler() {
}

void InstallExtensionHandler::GetLocalizedValues(
    content::WebUIDataSource* source) {
  source->AddString(
      "extensionSettingsInstallDropTarget",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALL_DROP_TARGET));
}

void InstallExtensionHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "startDrag",
      base::BindRepeating(&InstallExtensionHandler::HandleStartDragMessage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "stopDrag",
      base::BindRepeating(&InstallExtensionHandler::HandleStopDragMessage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "installDroppedFile",
      base::BindRepeating(&InstallExtensionHandler::HandleInstallMessage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "installDroppedDirectory",
      base::BindRepeating(
          &InstallExtensionHandler::HandleInstallDirectoryMessage,
          base::Unretained(this)));
}

void InstallExtensionHandler::HandleStartDragMessage(
    const base::ListValue* args) {
  content::DropData* drop_data =
      web_ui()->GetWebContents()->GetDropData();
  if (!drop_data) {
    DLOG(ERROR) << "No current drop data.";
    return;
  }

  if (drop_data->filenames.empty()) {
    DLOG(ERROR) << "Current drop data contains no files.";
    return;
  }

  const ui::FileInfo& file_info = drop_data->filenames.front();

  file_to_install_ = file_info.path;
  // Use the display name if provided, for checking file names
  // (.path is likely a random hash value in that case).
  file_display_name_ =
      file_info.display_name.empty() ? file_info.path : file_info.display_name;
}

void InstallExtensionHandler::HandleStopDragMessage(
    const base::ListValue* args) {
  file_to_install_.clear();
  file_display_name_.clear();
}

void InstallExtensionHandler::HandleInstallMessage(
    const base::ListValue* args) {
  if (file_to_install_.empty()) {
    LOG(ERROR) << "No file captured to install.";
    return;
  }

  Profile* profile = Profile::FromBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext());

  if (file_display_name_.MatchesExtension(FILE_PATH_LITERAL(".zip"))) {
    ZipFileInstaller::Create(
        content::ServiceManagerConnection::GetForProcess()->GetConnector(),
        MakeRegisterInExtensionServiceCallback(
            ExtensionSystem::Get(profile)->extension_service()))
        ->LoadFromZipFile(file_to_install_);
  } else {
    std::unique_ptr<ExtensionInstallPrompt> prompt(
        new ExtensionInstallPrompt(web_ui()->GetWebContents()));
    scoped_refptr<CrxInstaller> crx_installer(CrxInstaller::Create(
        ExtensionSystem::Get(profile)->extension_service(), std::move(prompt)));
    crx_installer->set_error_on_unsupported_requirements(true);
    crx_installer->set_off_store_install_allow_reason(
        CrxInstaller::OffStoreInstallAllowedFromSettingsPage);
    crx_installer->set_install_immediately(true);

    if (file_display_name_.MatchesExtension(FILE_PATH_LITERAL(".user.js"))) {
      crx_installer->InstallUserScript(
          file_to_install_, net::FilePathToFileURL(file_to_install_));
    } else if (file_display_name_.MatchesExtension(FILE_PATH_LITERAL(".crx"))) {
      crx_installer->InstallCrx(file_to_install_);
    } else {
      CHECK(false);
    }
  }

  file_to_install_.clear();
  file_display_name_.clear();
}

void InstallExtensionHandler::HandleInstallDirectoryMessage(
    const base::ListValue* args) {
  Profile* profile = Profile::FromBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext());
  UnpackedInstaller::Create(
      ExtensionSystem::Get(profile)->
          extension_service())->Load(file_to_install_);
}

}  // namespace extensions
