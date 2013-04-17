// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"

#include <string>

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// Reset screen id.
const char kResetScreen[] = "reset";

}  // namespace

namespace chromeos {

ResetScreenHandler::ResetScreenHandler()
    : delegate_(NULL), show_on_init_(false) {
}

ResetScreenHandler::~ResetScreenHandler() {
  if (delegate_)
    delegate_->OnActorDestroyed(this);
}

void ResetScreenHandler::PrepareToShow() {
}

void ResetScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kResetScreen, NULL);
}

void ResetScreenHandler::Hide() {
}

void ResetScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void ResetScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("resetScreenTitle", IDS_RESET_SCREEN_TITLE);
  builder->Add("cancelButton", IDS_CANCEL);

  builder->AddF("resetWarningText",
                IDS_RESET_SCREEN_WARNING_MSG,
                IDS_SHORT_PRODUCT_NAME);

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kFirstBoot)) {
    builder->AddF("resetWarningDetails",
                  IDS_RESET_SCREEN_WARNING_DETAILS,
                  IDS_SHORT_PRODUCT_NAME);
    builder->Add("resetButton", IDS_RESET_SCREEN_RESET);
  } else {
    builder->AddF("resetWarningDetails",
                  IDS_RESET_SCREEN_WARNING_DETAILS_RESTART,
                  IDS_SHORT_PRODUCT_NAME);
    builder->Add("resetButton", IDS_RELAUNCH_BUTTON);
  }
}

void ResetScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void ResetScreenHandler::RegisterMessages() {
  AddCallback("resetOnCancel", &ResetScreenHandler::HandleOnCancel);
  AddCallback("resetOnReset", &ResetScreenHandler::HandleOnReset);
}

void ResetScreenHandler::HandleOnCancel() {
  if (delegate_)
    delegate_->OnExit();
}

void ResetScreenHandler::HandleOnReset() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kFirstBoot)) {
    chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
        StartDeviceWipe();
  } else {
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kFactoryResetRequested, true);
    prefs->CommitPendingWrite();

    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
        RequestRestart();
  }
}

}  // namespace chromeos
