// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/host_pairing_screen_handler.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

namespace {

const char kJsScreenPath[] = "login.HostPairingScreen";

const char kMethodContextChanged[] = "contextChanged";

// Sent from JS when screen is ready to receive context updates.
// TODO(dzhioev): Move 'contextReady' logic to the base screen handler when
// all screens migrate to context-based communications.
const char kCallbackContextReady[] = "contextReady";

}  // namespace

HostPairingScreenHandler::HostPairingScreenHandler() {
  set_call_js_prefix(kJsScreenPath);
}

HostPairingScreenHandler::~HostPairingScreenHandler() {
  if (delegate_)
    delegate_->OnViewDestroyed(this);
}

void HostPairingScreenHandler::HandleContextReady() {
  js_context_ready_ = true;
  OnContextChanged(context_cache_.storage());
}

void HostPairingScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void HostPairingScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  // TODO(dzhioev): Move the prefix logic to the base screen handler after
  // migration.
  std::string prefix;
  base::RemoveChars(kJsScreenPath, ".", &prefix);

  // TODO(xdai): Clean up all unrelated strings and rename others if necessary.
  builder->Add(prefix + "WelcomeTitle", IDS_PAIRING_HOST_WELCOME_TITLE);
  builder->Add(prefix + "WelcomeText", IDS_PAIRING_HOST_WELCOME_TEXT);
  builder->Add(prefix + "ConfirmationTitle", IDS_SLAVE_CONFIRMATION_TITLE);
  builder->Add(prefix + "UpdatingTitle", IDS_PAIRING_HOST_UPDATING_TITLE);
  builder->Add(prefix + "UpdatingText", IDS_PAIRING_HOST_UPDATING_TEXT);
  builder->Add(prefix + "EnrollTitle", IDS_SLAVE_ENROLL_TITLE);
  builder->Add(prefix + "EnrollingTitle", IDS_SLAVE_ENROLLMENT_IN_PROGRESS);
  builder->Add(prefix + "DoneTitle", IDS_PAIRING_HOST_DONE_TITLE);
  builder->Add(prefix + "DoneText", IDS_PAIRING_HOST_DONE_TEXT);
  builder->Add(prefix + "EnrollmentErrorTitle",
               IDS_SLAVE_ENROLLMENT_ERROR_TITLE);
  builder->Add(prefix + "ErrorNeedsRestart",
               IDS_PAIRING_HOST_ERROR_NEED_RESTART_TEXT);
  builder->Add(prefix + "SetupBasicConfigTitle",
               IDS_HOST_SETUP_BASIC_CONFIGURATION_TITLE);
  builder->Add(prefix + "SetupNetworkErrorTitle",
               IDS_HOST_SETUP_NETWORK_ERROR_TITLE);
  builder->Add(prefix + "InitializationErrorTitle",
               IDS_PAIRING_HOST_INITIALIZATION_ERROR_TITLE);
  builder->Add(prefix + "ConnectionErrorTitle",
               IDS_PAIRING_HOST_CONNECTION_ERROR_TITLE);
  builder->Add(prefix + "ErrorNeedRestartText",
               IDS_PAIRING_HOST_ERROR_NEED_RESTART_TEXT);
  builder->Add(prefix + "ErrorNeedsRestart",
               IDS_PAIRING_HOST_ERROR_NEED_RESTART_TEXT);
}

void HostPairingScreenHandler::RegisterMessages() {
  AddPrefixedCallback(kCallbackContextReady,
                      &HostPairingScreenHandler::HandleContextReady);
}

void HostPairingScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(OobeScreen::SCREEN_OOBE_HOST_PAIRING);
}

void HostPairingScreenHandler::Hide() {
}

void HostPairingScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void HostPairingScreenHandler::OnContextChanged(
    const base::DictionaryValue& diff) {
  if (!js_context_ready_) {
    context_cache_.ApplyChanges(diff, NULL);
    return;
  }
  CallJS(kMethodContextChanged, diff);
}

}  // namespace chromeos
