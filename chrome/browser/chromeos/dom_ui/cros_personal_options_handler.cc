// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/cros_personal_options_handler.h"

#include "base/values.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/dom_ui/dom_ui_util.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

CrosPersonalOptionsHandler::CrosPersonalOptionsHandler() {
}

CrosPersonalOptionsHandler::~CrosPersonalOptionsHandler() {
}

// TODO(jhawkins): Move the contents of this file to PersonalOptionsHandler
// within an OS_CHROMEOS #ifdef.
void CrosPersonalOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("account",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PERSONAL_ACCOUNT_GROUP_NAME));
  localized_strings->SetString("enableScreenlock",
      l10n_util::GetStringUTF16(IDS_OPTIONS_ENABLE_SCREENLOCKER_CHECKBOX));
}

void CrosPersonalOptionsHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback(
      "loadAccountPicture",
      NewCallback(this, &CrosPersonalOptionsHandler::LoadAccountPicture));
}

void CrosPersonalOptionsHandler::LoadAccountPicture(const ListValue* args) {
  const SkBitmap& account_picture =
      UserManager::Get()->logged_in_user().image();

  if (!account_picture.isNull()) {
    StringValue data_url(dom_ui_util::GetImageDataUrl(account_picture));
    dom_ui_->CallJavascriptFunction(L"PersonalOptions.setAccountPicture",
        data_url);
  }
}

}  // namespace chromeos
