// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/wrong_hwid_screen_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

WrongHWIDScreenHandler::WrongHWIDScreenHandler()
    : delegate_(NULL), show_on_init_(false) {
}

WrongHWIDScreenHandler::~WrongHWIDScreenHandler() {
  if (delegate_)
    delegate_->OnActorDestroyed(this);
}

void WrongHWIDScreenHandler::PrepareToShow() {
}

void WrongHWIDScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(OobeUI::kScreenWrongHWID, NULL);
}

void WrongHWIDScreenHandler::Hide() {
}

void WrongHWIDScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void WrongHWIDScreenHandler::GetLocalizedStrings(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString(
      "wrongHWIDScreenHeader",
      l10n_util::GetStringUTF16(IDS_WRONG_HWID_SCREEN_HEADER));
  localized_strings->SetString(
      "wrongHWIDMessageFirstPart",
      l10n_util::GetStringUTF16(IDS_WRONG_HWID_SCREEN_MESSAGE_FIRST_PART));
  localized_strings->SetString(
      "wrongHWIDMessageSecondPart",
      l10n_util::GetStringUTF16(IDS_WRONG_HWID_SCREEN_MESSAGE_SECOND_PART));
  localized_strings->SetString(
      "wrongHWIDScreenSkipLink",
      l10n_util::GetStringUTF16(IDS_WRONG_HWID_SCREEN_SKIP_LINK));
}

void WrongHWIDScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void WrongHWIDScreenHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("wrongHWIDOnSkip",
      base::Bind(&WrongHWIDScreenHandler::HandleOnSkip,
                 base::Unretained(this)));
}

void WrongHWIDScreenHandler::HandleOnSkip(const base::ListValue* args) {
  if (delegate_)
    delegate_->OnExit();
}

}  // namespace chromeos

