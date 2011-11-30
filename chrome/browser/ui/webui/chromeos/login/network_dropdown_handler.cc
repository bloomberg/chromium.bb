// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_dropdown_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/ui/webui/chromeos/login/network_dropdown.h"
#include "content/browser/webui/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// JS API callbacks names.
const char kJsApiNetworkItemChosen[] = "networkItemChosen";
const char kJsApiNetworkDropdownShow[] = "networkDropdownShow";
const char kJsApiNetworkDropdownHide[] = "networkDropdownHide";
const char kJsApiNetworkDropdownRefresh[] = "networkDropdownRefresh";

}  // namespace

namespace chromeos {

NetworkDropdownHandler::NetworkDropdownHandler() {
}

NetworkDropdownHandler::~NetworkDropdownHandler() {
}

void NetworkDropdownHandler::GetLocalizedStrings(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString("selectNetwork",
      l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_SELECT));
}

void NetworkDropdownHandler::Initialize() {
}

void NetworkDropdownHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(kJsApiNetworkItemChosen,
      base::Bind(&NetworkDropdownHandler::HandleNetworkItemChosen,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback(kJsApiNetworkDropdownShow,
      base::Bind(&NetworkDropdownHandler::HandleNetworkDropdownShow,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback(kJsApiNetworkDropdownHide,
      base::Bind(&NetworkDropdownHandler::HandleNetworkDropdownHide,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback(kJsApiNetworkDropdownRefresh,
      base::Bind(&NetworkDropdownHandler::HandleNetworkDropdownRefresh,
                 base::Unretained(this)));
}

void NetworkDropdownHandler::HandleNetworkItemChosen(
    const base::ListValue* args) {
  DCHECK(args->GetSize() == 1);
  double id;
  if (!args->GetDouble(0, &id))
    NOTREACHED();
  DCHECK(dropdown_.get());
  dropdown_->OnItemChosen(static_cast<int>(id));
}

void NetworkDropdownHandler::HandleNetworkDropdownShow(
    const base::ListValue* args) {
  DCHECK(args->GetSize() == 3);
  std::string element_id;
  if (!args->GetString(0, &element_id))
    NOTREACHED();
  bool oobe;
  if (!args->GetBoolean(1, &oobe))
    NOTREACHED();


  double last_network_type = -1;  // Javascript passes integer as double.
  if (!args->GetDouble(2, &last_network_type))
    NOTREACHED();

  dropdown_.reset(new NetworkDropdown(web_ui_, GetNativeWindow(), oobe));

  if (last_network_type >= 0) {
    dropdown_->SetLastNetworkType(
        static_cast<ConnectionType>(last_network_type));
  }
}

void NetworkDropdownHandler::HandleNetworkDropdownHide(
    const base::ListValue* args) {
  DCHECK(args->GetSize() == 0);
  dropdown_.reset();
}

void NetworkDropdownHandler::HandleNetworkDropdownRefresh(
    const base::ListValue* args) {
  DCHECK(args->GetSize() == 0);
  // Since language change is async,
  // we may in theory be on another screen during this call.
  if (dropdown_.get())
    dropdown_->Refresh();
}

}  // namespace chromeos
