// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_handler.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_localized_strings_provider.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/multidevice_setup_resources.h"
#include "chrome/grit/multidevice_setup_resources_map.h"
#include "chromeos/grit/chromeos_resources.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/services/multidevice_setup/public/cpp/url_provider.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

constexpr int kPreferredDialogHeightPx = 640;
constexpr int kPreferredDialogWidthPx = 768;

}  // namespace

// static
MultiDeviceSetupDialog* MultiDeviceSetupDialog::current_instance_ = nullptr;

// static
void MultiDeviceSetupDialog::Show() {
  // The dialog is already showing, so there is nothing to do.
  if (current_instance_)
    return;

  current_instance_ = new MultiDeviceSetupDialog();
  gfx::NativeWindow window = chrome::ShowWebDialog(
      nullptr /* parent */, ProfileManager::GetActiveUserProfile(),
      current_instance_);

  // Remove the black backdrop behind the dialog window which appears in tablet
  // and full-screen mode.
  ash::WindowBackdrop::Get(window)->SetBackdropMode(
      ash::WindowBackdrop::BackdropMode::kDisabled);
}

// static
MultiDeviceSetupDialog* MultiDeviceSetupDialog::Get() {
  return current_instance_;
}

// static
void MultiDeviceSetupDialog::SetInstanceForTesting(
    MultiDeviceSetupDialog* instance) {
  current_instance_ = instance;
}

void MultiDeviceSetupDialog::AddOnCloseCallback(base::OnceClosure callback) {
  on_close_callbacks_.push_back(std::move(callback));
}

MultiDeviceSetupDialog::MultiDeviceSetupDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIMultiDeviceSetupUrl),
                              base::string16()) {}

MultiDeviceSetupDialog::~MultiDeviceSetupDialog() {
  for (auto& callback : on_close_callbacks_)
    std::move(callback).Run();
}

void MultiDeviceSetupDialog::GetDialogSize(gfx::Size* size) const {
  // Note: The size is calculated once based on the current screen orientation
  // and is not ever updated. It might be possible to resize the dialog upon
  // each screen rotation, but https://crbug.com/1030993 prevents this from
  // working.
  // TODO(https://crbug.com/1030993): Explore resizing the dialog dynamically.
  static const gfx::Size dialog_size = ComputeDialogSizeForInternalScreen(
      gfx::Size(kPreferredDialogWidthPx, kPreferredDialogHeightPx));
  size->SetSize(dialog_size.width(), dialog_size.height());
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

  chromeos::multidevice_setup::AddLocalizedStrings(source);
  source->UseStringsJs();
  source->SetDefaultResource(
      IDR_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_DIALOG_HTML);

  // Note: The |kMultiDeviceSetupResourcesSize| and |kMultideviceSetupResources|
  // fields are defined in the generated file
  // chrome/grit/multidevice_setup_resources_map.h.
  for (size_t i = 0; i < kMultideviceSetupResourcesSize; ++i) {
    source->AddResourcePath(kMultideviceSetupResources[i].name,
                            kMultideviceSetupResources[i].value);
  }

  web_ui->AddMessageHandler(std::make_unique<MultideviceSetupHandler>());
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

MultiDeviceSetupDialogUI::~MultiDeviceSetupDialogUI() = default;

void MultiDeviceSetupDialogUI::BindInterface(
    mojo::PendingReceiver<chromeos::multidevice_setup::mojom::MultiDeviceSetup>
        receiver) {
  MultiDeviceSetupService* service =
      MultiDeviceSetupServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (service)
    service->BindMultiDeviceSetup(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(MultiDeviceSetupDialogUI)

}  // namespace multidevice_setup

}  // namespace chromeos
