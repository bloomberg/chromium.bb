// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/wrong_hwid_screen_handler.h"

#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

const char kJsScreenPath[] = "login.WrongHWIDScreen";

}  // namespace

namespace chromeos {

WrongHWIDScreenHandler::WrongHWIDScreenHandler()
    : BaseScreenHandler(kJsScreenPath),
      delegate_(NULL),
      show_on_init_(false) {
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

void WrongHWIDScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("wrongHWIDScreenHeader", IDS_WRONG_HWID_SCREEN_HEADER);
  builder->Add("wrongHWIDMessageFirstPart",
                IDS_WRONG_HWID_SCREEN_MESSAGE_FIRST_PART);
  builder->Add("wrongHWIDMessageSecondPart",
                IDS_WRONG_HWID_SCREEN_MESSAGE_SECOND_PART);
  builder->Add("wrongHWIDScreenSkipLink",
                IDS_WRONG_HWID_SCREEN_SKIP_LINK);
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
  AddCallback("wrongHWIDOnSkip", &WrongHWIDScreenHandler::HandleOnSkip);
}

void WrongHWIDScreenHandler::HandleOnSkip() {
  if (delegate_)
    delegate_->OnExit();
}

}  // namespace chromeos
