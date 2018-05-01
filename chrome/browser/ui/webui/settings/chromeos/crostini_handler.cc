// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/crostini_handler.h"

#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/crostini/crostini_installer_view.h"
#include "chrome/browser/ui/app_list/crostini/crostini_uninstaller_view.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace settings {

CrostiniHandler::CrostiniHandler() : weak_ptr_factory_(this) {}

CrostiniHandler::~CrostiniHandler() = default;

void CrostiniHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestCrostiniInstallerView",
      base::BindRepeating(&CrostiniHandler::HandleRequestCrostiniInstallerView,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestRemoveCrostini",
      base::BindRepeating(&CrostiniHandler::HandleRequestRemoveCrostini,
                          weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::HandleRequestCrostiniInstallerView(
    const base::ListValue* args) {
  AllowJavascript();
  CrostiniInstallerView::Show(Profile::FromWebUI(web_ui()));
}

void CrostiniHandler::HandleRequestRemoveCrostini(const base::ListValue* args) {
  AllowJavascript();
  // TODO(nverne): change this to use CrostiniUninstallerView::Show
  crostini::CrostiniManager::GetInstance()->RemoveCrostini(
      Profile::FromWebUI(web_ui()), kCrostiniDefaultVmName,
      kCrostiniDefaultContainerName, base::DoNothing());
}

}  // namespace settings
}  // namespace chromeos
