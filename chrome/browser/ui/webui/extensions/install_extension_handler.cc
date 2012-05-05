// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/install_extension_handler.h"

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_switch_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/webdropdata.h"

InstallExtensionHandler::InstallExtensionHandler() {
}

InstallExtensionHandler::~InstallExtensionHandler() {
}

void InstallExtensionHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString(
      "extensionSettingsInstallDropTarget",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALL_DROP_TARGET));
  localized_strings->SetBoolean(
      "offStoreInstallEnabled",
      extensions::switch_utils::IsOffStoreInstallEnabled());
}

void InstallExtensionHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "startDrag",
      base::Bind(&InstallExtensionHandler::HandleStartDragMessage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "stopDrag",
      base::Bind(&InstallExtensionHandler::HandleStopDragMessage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "installDroppedFile",
      base::Bind(&InstallExtensionHandler::HandleInstallMessage,
                 base::Unretained(this)));
}

void InstallExtensionHandler::HandleStartDragMessage(const ListValue* args) {
  WebDropData* drop_data = web_ui()->GetWebContents()->GetView()->GetDropData();
  if (!drop_data) {
    DLOG(ERROR) << "No current drop data.";
    return;
  }

  if (drop_data->filenames.empty()) {
    DLOG(ERROR) << "Current drop data contains no files.";
    return;
  }

  file_to_install_ = FilePath::FromWStringHack(
      UTF16ToWide(drop_data->filenames.front()));
}

void InstallExtensionHandler::HandleStopDragMessage(const ListValue* args) {
  file_to_install_.clear();
}

void InstallExtensionHandler::HandleInstallMessage(const ListValue* args) {
  if (file_to_install_.empty()) {
    LOG(ERROR) << "No file captured to install.";
    return;
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  scoped_refptr<CrxInstaller> crx_installer(
      CrxInstaller::Create(
          ExtensionSystem::Get(profile)->extension_service(),
          new ExtensionInstallUI(profile)));
  crx_installer->InstallCrx(file_to_install_);

  file_to_install_.clear();
}
