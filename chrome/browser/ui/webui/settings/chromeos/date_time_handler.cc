// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/date_time_handler.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace settings {

namespace {

// Returns whether the system's automatic time zone detection setting is
// managed, which may override the user's setting.
bool IsSystemTimezoneAutomaticDetectionManaged() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSystemTimezoneAutomaticDetectionPolicy)) {
    return false;
  }
  return g_browser_process->local_state()->IsManagedPreference(
      prefs::kSystemTimezoneAutomaticDetectionPolicy);
}

// Returns the system's current automatic time zone detection policy value,
// which corresponds to the SystemTimezoneProto's AutomaticTimezoneDetectionType
// enum and determines whether the user's setting will be overridden.
int GetSystemTimezoneAutomaticDetectionPolicyValue() {
  DCHECK(IsSystemTimezoneAutomaticDetectionManaged());

  return g_browser_process->local_state()->GetInteger(
      prefs::kSystemTimezoneAutomaticDetectionPolicy);
}

}  // namespace

DateTimeHandler::DateTimeHandler() {}

DateTimeHandler::~DateTimeHandler() {}

DateTimeHandler* DateTimeHandler::Create(
    content::WebUIDataSource* html_source) {
  html_source->AddBoolean("systemTimeZoneManaged",
      chromeos::system::HasSystemTimezonePolicy());

  bool system_time_zone_automatic_detection_managed =
      IsSystemTimezoneAutomaticDetectionManaged();
  html_source->AddBoolean("systemTimeZoneDetectionManaged",
      system_time_zone_automatic_detection_managed);
  if (system_time_zone_automatic_detection_managed) {
    base::DictionaryValue dict;
    dict.SetInteger("systemTimeZoneDetectionPolicyValue",
        GetSystemTimezoneAutomaticDetectionPolicyValue());
    html_source->AddLocalizedStrings(dict);
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableTimeZoneTrackingOption)) {
    html_source->AddBoolean("hideTimeZoneDetection", true);
  }

  return new DateTimeHandler;
}

void DateTimeHandler::RegisterMessages() {
  // TODO(michaelpg): Add time zone message handlers.
}

void DateTimeHandler::OnJavascriptAllowed() {
  // TODO(michaelpg): Add policy observers.
}

void DateTimeHandler::OnJavascriptDisallowed() {
  // TODO(michaelpg): Remove policy observers.
}

}  // namespace settings
}  // namespace chromeos
