// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_dropdown_handler.h"

#include "base/values.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/ui/webui/chromeos/login/network_dropdown.h"
#include "grit/generated_resources.h"

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

void NetworkDropdownHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
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
}

void NetworkDropdownHandler::HandleNetworkItemChosen(
    const base::ListValue* args) {
  DCHECK(args->GetSize() == 1);

  double id;
  if (!args->GetDouble(0, &id)) {
    NOTREACHED();
    return;
  }

  if (dropdown_.get()) {
    dropdown_->OnItemChosen(static_cast<int>(id));
  } else {
    // It could happen with very low probability but still keep NOTREACHED to
    // detect if it starts happening all the time.
    NOTREACHED();
  }
}

void NetworkDropdownHandler::HandleNetworkDropdownShow(
    const base::ListValue* args) {
  DCHECK(args->GetSize() == 3);
  std::string element_id;
  if (!args->GetString(0, &element_id)) {
    NOTREACHED();
    return;
  }

  bool oobe;
  if (!args->GetBoolean(1, &oobe)) {
    NOTREACHED();
    return;
  }

  double last_network_type = -1;  // Javascript passes integer as double.
  if (!args->GetDouble(2, &last_network_type)) {
    NOTREACHED();
    return;
  }

  dropdown_.reset(new NetworkDropdown(web_ui(), oobe));

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
