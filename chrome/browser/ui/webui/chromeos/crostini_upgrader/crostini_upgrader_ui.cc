// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_ui.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/crostini/crostini_upgrader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

bool CrostiniUpgraderUI::IsEnabled() {
  return base::FeatureList::IsEnabled(
      chromeos::features::kCrostiniWebUIUpgrader);
}

void AddStringResources(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"upgrade", IDS_CROSTINI_UPGRADER_UPGRADE_BUTTON},
      {"retry", IDS_CROSTINI_INSTALLER_RETRY_BUTTON},
      {"close", IDS_APP_CLOSE},
      {"cancel", IDS_APP_CANCEL},
      {"done", IDS_CROSTINI_UPGRADER_DONE_BUTTON},
      {"restore", IDS_CROSTINI_UPGRADER_RESTORE_BUTTON},
      {"learnMore", IDS_LEARN_MORE},

      {"promptTitle", IDS_CROSTINI_UPGRADER_TITLE},
      {"backingUpTitle", IDS_CROSTINI_UPGRADER_BACKING_UP_TITLE},
      {"backupSucceededTitle", IDS_CROSTINI_UPGRADER_BACKUP_SUCCEEDED_TITLE},
      {"prechecksFailedTitle", IDS_CROSTINI_UPGRADER_PRECHECKS_FAILED_TITLE},
      {"upgradingTitle", IDS_CROSTINI_UPGRADER_UPGRADING_TITLE},
      {"restoreTitle", IDS_CROSTINI_UPGRADER_RESTORE_TITLE},
      {"restoreSucceededTitle", IDS_CROSTINI_UPGRADER_RESTORE_SUCCEEDED_TITLE},
      {"succeededTitle", IDS_CROSTINI_UPGRADER_SUCCEEDED_TITLE},
      {"cancelingTitle", IDS_CROSTINI_UPGRADER_CANCELING_TITLE},
      {"errorTitle", IDS_CROSTINI_UPGRADER_ERROR_TITLE},

      {"precheckNoNetwork", IDS_CROSTINI_UPGRADER_PRECHECKS_FAILED_NETWORK},
      {"precheckNoPower", IDS_CROSTINI_UPGRADER_PRECHECKS_FAILED_POWER},

      {"promptMessage", IDS_CROSTINI_UPGRADER_BODY},
      {"backingUpMessage", IDS_CROSTINI_UPGRADER_BACKING_UP_MESSAGE},
      {"backupSucceededMessage",
       IDS_CROSTINI_UPGRADER_BACKUP_SUCCEEDED_MESSAGE},
      {"upgradingMessage", IDS_CROSTINI_UPGRADER_UPGRADING},
      {"succeededMessage", IDS_CROSTINI_UPGRADER_SUCCEEDED},
      {"cancelingMessage", IDS_CROSTINI_UPGRADER_CANCELING},
      {"offerRestoreMessage", IDS_CROSTINI_UPGRADER_OFFER_RESTORE_MESSAGE},
      {"restoreMessage", IDS_CROSTINI_UPGRADER_RESTORE_MESSAGE},
      {"restoreSucceededMessage",
       IDS_CROSTINI_UPGRADER_RESTORE_SUCCEEDED_MESSAGE},

      {"backupCheckboxMessage", IDS_CROSTINI_UPGRADER_BACKUP_CHECKBOX_MESSAGE},
      {"backupChangeLocation", IDS_CROSTINI_UPGRADER_BACKUP_CHANGE_LOCATION},
  };
  AddLocalizedStringsBulk(source, kStrings);
  source->AddString("learnMoreUrl",
                    std::string{chrome::kLinuxAppsLearnMoreURL} +
                        "&b=" + base::SysInfo::GetLsbReleaseBoard());

  base::string16 device_name = ui::GetChromeOSDeviceName();
  source->AddString("offlineError",
                    l10n_util::GetStringFUTF8(
                        IDS_CROSTINI_INSTALLER_OFFLINE_ERROR, device_name));
  source->AddString("precheckNoSpace",
                    l10n_util::GetStringFUTF8(
                        IDS_CROSTINI_UPGRADER_PRECHECKS_FAILED_SPACE,
                        ui::FormatBytesWithUnits(
                            crostini::CrostiniUpgrader::kDiskRequired,
                            ui::DATA_UNITS_GIBIBYTE, /*show_units=*/true)));
}

CrostiniUpgraderUI::CrostiniUpgraderUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI{web_ui} {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUICrostiniUpgraderHost);
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources chrome://test 'self';");
  AddStringResources(source);

  source->AddResourcePath("images/linux_illustration.png",
                          IDR_LINUX_ILLUSTRATION);
  source->AddResourcePath("images/success_illustration.svg",
                          IDR_LINUX_SUCCESS_ILLUSTRATION);
  source->AddResourcePath("images/error_illustration.png",
                          IDR_PLUGIN_VM_INSTALLER_ERROR);
  source->AddResourcePath("app.js", IDR_CROSTINI_UPGRADER_APP_JS);
  source->AddResourcePath("browser_proxy.js",
                          IDR_CROSTINI_UPGRADER_BROWSER_PROXY_JS);
  source->AddResourcePath("crostini_upgrader.mojom-lite.js",
                          IDR_CROSTINI_UPGRADER_MOJO_LITE_JS);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER);
  source->SetDefaultResource(IDR_CROSTINI_UPGRADER_INDEX_HTML);
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

CrostiniUpgraderUI::~CrostiniUpgraderUI() = default;

void CrostiniUpgraderUI::BindInterface(
    mojo::PendingReceiver<
        chromeos::crostini_upgrader::mojom::PageHandlerFactory>
        pending_receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }

  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void CrostiniUpgraderUI::CreatePageHandler(
    mojo::PendingRemote<chromeos::crostini_upgrader::mojom::Page> pending_page,
    mojo::PendingReceiver<chromeos::crostini_upgrader::mojom::PageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());

  page_handler_ = std::make_unique<CrostiniUpgraderPageHandler>(
      web_ui()->GetWebContents(),
      crostini::CrostiniUpgrader::GetForProfile(Profile::FromWebUI(web_ui())),
      std::move(pending_page_handler), std::move(pending_page),
      // Using Unretained(this) because |page_handler_| will not out-live
      // |this|.
      base::BindOnce(&CrostiniUpgraderUI::OnWebUICloseDialog,
                     base::Unretained(this)),
      std::move(launch_callback_));
}

void CrostiniUpgraderUI::OnWebUICloseDialog() {
  can_close_ = true;
  // CloseDialog() is a no-op if we are not in a dialog (e.g. user
  // access the page using the URL directly, which is not supported).
  ui::MojoWebDialogUI::CloseDialog(nullptr);
}

WEB_UI_CONTROLLER_TYPE_IMPL(CrostiniUpgraderUI)

}  // namespace chromeos
