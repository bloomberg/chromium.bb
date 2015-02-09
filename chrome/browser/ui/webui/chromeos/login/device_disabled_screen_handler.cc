// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/device_disabled_screen_handler.h"

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace {

const char kJsScreenPath[] = "login.DeviceDisabledScreen";

}  // namespace

namespace chromeos {

DeviceDisabledScreenHandler::DeviceDisabledScreenHandler()
    : BaseScreenHandler(kJsScreenPath),
      delegate_(NULL),
      show_on_init_(false) {
}

DeviceDisabledScreenHandler::~DeviceDisabledScreenHandler() {
  if (delegate_)
    delegate_->OnActorDestroyed(this);
}

void DeviceDisabledScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  if (delegate_) {
    CallJS("setEnrollmentDomain", delegate_->GetEnrollmentDomain());
    CallJS("setMessage", delegate_->GetMessage());
  }
  ShowScreen(OobeUI::kScreenDeviceDisabled, NULL);
}

void DeviceDisabledScreenHandler::Hide() {
  show_on_init_ = false;
}

void DeviceDisabledScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void DeviceDisabledScreenHandler::UpdateMessage(const std::string& message) {
  if (page_is_ready())
    CallJS("setMessage", message);
}

void DeviceDisabledScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("deviceDisabledHeading", IDS_DEVICE_DISABLED_HEADING);
  builder->Add("deviceDisabledExplanationWithDomain",
               IDS_DEVICE_DISABLED_EXPLANATION_WITH_DOMAIN);
  builder->Add("deviceDisabledExplanationWithoutDomain",
               IDS_DEVICE_DISABLED_EXPLANATION_WITHOUT_DOMAIN);
}

void DeviceDisabledScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void DeviceDisabledScreenHandler::RegisterMessages() {
}

}  // namespace chromeos
