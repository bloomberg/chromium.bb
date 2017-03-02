// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/date_time_options_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/chromeos/set_time_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/system_clock_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace options {

DateTimeOptionsHandler::DateTimeOptionsHandler()
    : can_set_time_(false), page_initialized_(false) {
}

DateTimeOptionsHandler::~DateTimeOptionsHandler() {
  DBusThreadManager::Get()->GetSystemClockClient()->RemoveObserver(this);
}

void DateTimeOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString(
      "setTimeButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SET_TIME_BUTTON));
}

void DateTimeOptionsHandler::InitializeHandler() {
  SystemClockClient* system_clock_client =
      DBusThreadManager::Get()->GetSystemClockClient();
  system_clock_client->AddObserver(this);

  can_set_time_ = system_clock_client->CanSetTime();
  SystemClockCanSetTimeChanged(can_set_time_);
}

void DateTimeOptionsHandler::InitializePage() {
  page_initialized_ = true;
  SystemClockCanSetTimeChanged(can_set_time_);
}

void DateTimeOptionsHandler::RegisterMessages() {
  // Callback for set time button.
  web_ui()->RegisterMessageCallback(
      "showSetTime",
      base::Bind(&DateTimeOptionsHandler::HandleShowSetTime,
                 base::Unretained(this)));
}

void DateTimeOptionsHandler::SystemClockCanSetTimeChanged(bool can_set_time) {
  if (page_initialized_) {
    web_ui()->CallJavascriptFunctionUnsafe("BrowserOptions.setCanSetTime",
                                           base::Value(can_set_time));
  }
  can_set_time_ = can_set_time;
}

void DateTimeOptionsHandler::HandleShowSetTime(const base::ListValue* args) {
  // Make sure the clock status hasn't changed since the button was clicked.
  if (can_set_time_) {
    SetTimeDialog::ShowDialogInParent(
        web_ui()->GetWebContents()->GetTopLevelNativeWindow());
  }
}

}  // namespace options
}  // namespace chromeos
