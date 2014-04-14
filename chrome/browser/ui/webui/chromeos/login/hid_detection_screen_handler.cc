// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

const char kJsScreenPath[] = "login.HIDDetectionScreen";

}  // namespace

namespace chromeos {

HIDDetectionScreenHandler::HIDDetectionScreenHandler()
    : BaseScreenHandler(kJsScreenPath),
      delegate_(NULL),
      show_on_init_(false) {
}

HIDDetectionScreenHandler::~HIDDetectionScreenHandler() {
  if (delegate_)
    delegate_->OnActorDestroyed(this);
}

void HIDDetectionScreenHandler::PrepareToShow() {
}

void HIDDetectionScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(OobeUI::kScreenHIDDetection, NULL);
}

void HIDDetectionScreenHandler::Hide() {
}

void HIDDetectionScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void HIDDetectionScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("hidDetectionContinue", IDS_HID_DETECTION_CONTINUE_BUTTON);
}

void HIDDetectionScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void HIDDetectionScreenHandler::RegisterMessages() {
  AddCallback(
      "HIDDetectionOnContinue", &HIDDetectionScreenHandler::HandleOnContinue);
}

void HIDDetectionScreenHandler::HandleOnContinue() {
  if (delegate_)
    delegate_->OnExit();
}

}  // namespace chromeos
