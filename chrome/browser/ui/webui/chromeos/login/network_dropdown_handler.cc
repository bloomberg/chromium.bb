// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_dropdown_handler.h"

#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/ui/choose_mobile_network_dialog.h"
#include "chrome/browser/ui/webui/chromeos/login/network_dropdown.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

const char kJsScreenPath[] = "cr.ui.DropDown";

// JS API callbacks names.
const char kJsApiNetworkItemChosen[] = "networkItemChosen";
const char kJsApiNetworkDropdownShow[] = "networkDropdownShow";
const char kJsApiNetworkDropdownHide[] = "networkDropdownHide";
const char kJsApiNetworkDropdownRefresh[] = "networkDropdownRefresh";
const char kJsApiLaunchProxySettingsDialog[] = "launchProxySettingsDialog";
const char kJsApiLaunchAddWiFiNetworkDialog[] = "launchAddWiFiNetworkDialog";
const char kJsApiLaunchAddMobileNetworkDialog[] =
    "launchAddMobileNetworkDialog";

}  // namespace

namespace chromeos {

NetworkDropdownHandler::NetworkDropdownHandler() {
  set_call_js_prefix(kJsScreenPath);
}

NetworkDropdownHandler::~NetworkDropdownHandler() {
}

void NetworkDropdownHandler::AddObserver(Observer* observer) {
  if (observer && !observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void NetworkDropdownHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NetworkDropdownHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("selectNetwork", IDS_NETWORK_SELECTION_SELECT);
  builder->Add("selectAnotherNetwork", IDS_ANOTHER_NETWORK_SELECTION_SELECT);
}

void NetworkDropdownHandler::Initialize() {
}

void NetworkDropdownHandler::RegisterMessages() {
  AddCallback(kJsApiNetworkItemChosen,
              &NetworkDropdownHandler::HandleNetworkItemChosen);
  AddCallback(kJsApiNetworkDropdownShow,
              &NetworkDropdownHandler::HandleNetworkDropdownShow);
  AddCallback(kJsApiNetworkDropdownHide,
              &NetworkDropdownHandler::HandleNetworkDropdownHide);
  AddCallback(kJsApiNetworkDropdownRefresh,
              &NetworkDropdownHandler::HandleNetworkDropdownRefresh);

  // MD-OOBE
  AddCallback(kJsApiLaunchProxySettingsDialog,
              &NetworkDropdownHandler::HandleLaunchProxySettingsDialog);
  AddCallback(kJsApiLaunchAddWiFiNetworkDialog,
              &NetworkDropdownHandler::HandleLaunchAddWiFiNetworkDialog);
  AddCallback(kJsApiLaunchAddMobileNetworkDialog,
              &NetworkDropdownHandler::HandleLaunchAddMobileNetworkDialog);
}

void NetworkDropdownHandler::HandleLaunchProxySettingsDialog() {
  dropdown_->OpenButtonOptions();
}

void NetworkDropdownHandler::HandleLaunchAddWiFiNetworkDialog() {
  gfx::NativeWindow native_window = GetNativeWindow();
  NetworkConfigView::ShowForType(shill::kTypeWifi, native_window);
}

void NetworkDropdownHandler::HandleLaunchAddMobileNetworkDialog() {
  gfx::NativeWindow native_window = GetNativeWindow();
  ChooseMobileNetworkDialog::ShowDialog(native_window);
}

void NetworkDropdownHandler::OnConnectToNetworkRequested() {
  for (Observer& observer : observers_)
    observer.OnConnectToNetworkRequested();
}

void NetworkDropdownHandler::HandleNetworkItemChosen(double id) {
  if (dropdown_.get()) {
    dropdown_->OnItemChosen(static_cast<int>(id));
  } else {
    // It could happen with very low probability but still keep NOTREACHED to
    // detect if it starts happening all the time.
    NOTREACHED();
  }
}

void NetworkDropdownHandler::HandleNetworkDropdownShow(
    const std::string& element_id,
    bool oobe) {
  dropdown_.reset(new NetworkDropdown(this, web_ui(), oobe));
}

void NetworkDropdownHandler::HandleNetworkDropdownHide() {
  dropdown_.reset();
}

void NetworkDropdownHandler::HandleNetworkDropdownRefresh() {
  // Since language change is async,
  // we may in theory be on another screen during this call.
  if (dropdown_.get())
    dropdown_->Refresh();
}

}  // namespace chromeos
