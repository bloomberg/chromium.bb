// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/multidevice_setup_resources.h"
#include "chrome/grit/multidevice_setup_resources_map.h"
#include "chromeos/grit/chromeos_resources.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

// TODO(khorimoto): Localize footnote marker if necessary.
const char kFootnoteMarker[] = "*";

constexpr int kDialogHeightPx = 640;
constexpr int kDialogWidthPx = 768;

void AddMultiDeviceSetupStrings(content::WebUIDataSource* html_source) {
  static constexpr struct {
    const char* name;
    int id;
  } kLocalizedStringsWithoutPlaceholders[] = {
      {"accept", IDS_MULTIDEVICE_SETUP_ACCEPT_LABEL},
      {"cancel", IDS_CANCEL},
      {"done", IDS_DONE},
      {"passwordPageHeader", IDS_MULTIDEVICE_SETUP_PASSWORD_PAGE_HEADER},
      {"enterPassword",
       IDS_MULTIDEVICE_SETUP_PASSWORD_PAGE_ENTER_PASSWORD_LABEL},
      {"wrongPassword",
       IDS_MULTIDEVICE_SETUP_PASSWORD_PAGE_WRONG_PASSWORD_LABEL},
      {"startSetupPageMultipleDeviceHeader",
       IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_MULTIPLE_DEVICE_HEADER},
      {"startSetupPageSingleDeviceHeader",
       IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_SINGLE_DEVICE_HEADER},
      {"startSetupPageFeatureListHeader",
       IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_FEATURE_LIST_HEADER},
      {"startSetupPageFeatureListAwm",
       IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_AWM_DESCRIPTION},
      {"startSetupPageFeatureListInstallApps",
       IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_INSTALL_APPS_DESCRIPTION},
      {"startSetupPageFeatureListAddFeatures",
       IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_ADD_FEATURES},
      {"setupFailedPageHeader", IDS_MULTIDEVICE_SETUP_SETUP_FAILED_PAGE_HEADER},
      {"setupFailedPageMessage",
       IDS_MULTIDEVICE_SETUP_SETUP_FAILED_PAGE_MESSAGE},
      {"setupSucceededPageHeader",
       IDS_MULTIDEVICE_SETUP_SETUP_SUCCEEDED_PAGE_HEADER},
      {"setupSucceededPageMessage",
       IDS_MULTIDEVICE_SETUP_SETUP_SUCCEEDED_PAGE_MESSAGE},
      {"startSetupPageHeader", IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_HEADER},
      {"title", IDS_MULTIDEVICE_SETUP_DIALOG_TITLE},
      {"tryAgain", IDS_MULTIDEVICE_SETUP_TRY_AGAIN_LABEL},
  };

  for (const auto& entry : kLocalizedStringsWithoutPlaceholders)
    html_source->AddLocalizedString(entry.name, entry.id);

  html_source->AddString(
      "startSetupPageMessage",
      l10n_util::GetStringFUTF16(
          IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_MESSAGE,
          base::ASCIIToUTF16(kFootnoteMarker),
          base::ASCIIToUTF16(chrome::kMultiDeviceLearnMoreURL)));
  html_source->AddString(
      "startSetupPageFootnote",
      l10n_util::GetStringFUTF16(
          IDS_MULTIDEVICE_SETUP_START_SETUP_PAGE_FOOTNOTE,
          base::ASCIIToUTF16(kFootnoteMarker)));
}

}  // namespace

// static
MultiDeviceSetupDialog* MultiDeviceSetupDialog::current_instance_ = nullptr;

// static
void MultiDeviceSetupDialog::Show() {
  // The dialog is already showing, so there is nothing to do.
  if (current_instance_)
    return;

  current_instance_ = new MultiDeviceSetupDialog();
  current_instance_->ShowSystemDialog();
}

MultiDeviceSetupDialog::MultiDeviceSetupDialog()
    : SystemWebDialogDelegate(
          GURL(chrome::kChromeUIMultiDeviceSetupUrl),
          l10n_util::GetStringUTF16(IDS_MULTIDEVICE_SETUP_DIALOG_TITLE)) {}

MultiDeviceSetupDialog::~MultiDeviceSetupDialog() = default;

void MultiDeviceSetupDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDialogWidthPx, kDialogHeightPx);
}

void MultiDeviceSetupDialog::OnDialogClosed(const std::string& json_retval) {
  DCHECK(this == current_instance_);
  current_instance_ = nullptr;

  // Note: The call below deletes |this|, so there is no further need to keep
  // track of the pointer.
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

MultiDeviceSetupDialogUI::MultiDeviceSetupDialogUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIMultiDeviceSetupHost);

  AddMultiDeviceSetupStrings(source);
  source->SetJsonPath("strings.js");
  source->SetDefaultResource(
      IDR_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_DIALOG_HTML);
  source->AddResourcePath("mojo/public/mojom/base/time.mojom.js",
                          IDR_TIME_MOJOM_JS);
  source->AddResourcePath(
      "chromeos/services/device_sync/public/mojom/device_sync.mojom.js",
      IDR_DEVICE_SYNC_MOJOM_JS);
  source->AddResourcePath(
      "chromeos/services/multidevice_setup/public/mojom/"
      "multidevice_setup.mojom.js",
      IDR_MULTIDEVICE_SETUP_MOJOM_JS);
  source->AddResourcePath(
      "chromeos/services/multidevice_setup/public/mojom/"
      "multidevice_setup_constants.mojom.js",
      IDR_MULTIDEVICE_SETUP_CONSTANTS_MOJOM_JS);

  // Note: The |kMultiDeviceSetupResourcesSize| and |kMultideviceSetupResources|
  // fields are defined in the generated file
  // chrome/grit/multidevice_setup_resources_map.h.
  for (size_t i = 0; i < kMultideviceSetupResourcesSize; ++i) {
    source->AddResourcePath(kMultideviceSetupResources[i].name,
                            kMultideviceSetupResources[i].value);
  }

  web_ui->AddMessageHandler(std::make_unique<MultideviceSetupHandler>());
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);

  // Add Mojo bindings to this WebUI so that Mojo calls can occur in JavaScript.
  AddHandlerToRegistry(base::BindRepeating(
      &MultiDeviceSetupDialogUI::BindMultiDeviceSetup, base::Unretained(this)));
}

MultiDeviceSetupDialogUI::~MultiDeviceSetupDialogUI() = default;

void MultiDeviceSetupDialogUI::BindMultiDeviceSetup(
    chromeos::multidevice_setup::mojom::MultiDeviceSetupRequest request) {
  service_manager::Connector* connector =
      content::BrowserContext::GetConnectorFor(
          web_ui()->GetWebContents()->GetBrowserContext());
  DCHECK(connector);

  connector->BindInterface(chromeos::multidevice_setup::mojom::kServiceName,
                           std::move(request));
}

}  // namespace multidevice_setup

}  // namespace chromeos
