// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi/gcapi_omaha_experiment.h"

#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/win/registry.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/gcapi/gcapi.h"

using base::Time;
using base::TimeDelta;
using base::win::RegKey;

namespace {

const wchar_t kExperimentLabels[] = L"experiment_labels";

const wchar_t* kExperimentAppGuids[] = {
    L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}",
    L"{8A69D345-D564-463C-AFF1-A69D9E530F96}",
};

const wchar_t* kDays[] =
    { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };

const wchar_t* kMonths[] = {L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
                            L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"};

// Constructs a date string of the following format for the current time plus
// one year:
// "DAY, DD0 MON YYYY HH0:MI0:SE0 TZ"
//  DAY = 3 character day of week,
//  DD0 = 2 digit day of month,
//  MON = 3 character month of year,
//  YYYY = 4 digit year,
//  HH0 = 2 digit hour,
//  MI0 = 2 digit minute,
//  SE0 = 2 digit second,
//  TZ = 3 character timezone)
string16 BuildOmahaExperimentDateString() {
  Time then_time = Time::Now() + TimeDelta::FromDays(365);
  Time::Exploded then = {};
  then_time.UTCExplode(&then);

  if (!then.HasValidValues())
    return L"";

  string16 date_string;
  base::SStringPrintf(&date_string,
                      L"%ls, %02d %ls %d %02d:%02d:%02d GMT",
                      kDays[then.day_of_week],
                      then.day_of_month,
                      kMonths[then.month - 1],
                      then.year,
                      then.hour,
                      then.minute,
                      then.second);
  return date_string;
}

// Returns the number of weeks since 2/3/2003.
int GetCurrentRlzWeek() {
  Time::Exploded february_third_2003_exploded = {2003, 2, 1, 3, 0, 0, 0, 0};
  Time f = Time::FromUTCExploded(february_third_2003_exploded);
  TimeDelta delta = Time::Now() - f;
  return delta.InDays() / 7;
}

}  // namespace

bool SetOmahaExperimentLabel(const wchar_t* brand_code, int shell_mode) {
  if (!brand_code) {
    return false;
  }

  // When this function is invoked in standard, non-elevated shell, we default
  // to writing the experiment label to HKCU.  When it is invoked in a UAC-
  // elevated shell, we write the experiment label to HKLM.
  HKEY registry_hive =
      shell_mode == GCAPI_INVOKED_UAC_ELEVATION ? HKEY_LOCAL_MACHINE :
                                                  HKEY_CURRENT_USER;

  int week_number = GetCurrentRlzWeek();
  if (week_number < 0 || week_number > 999)
    week_number = 999;

  string16 experiment_label;
  base::SStringPrintf(&experiment_label,
                      L"reacbrand=%ls_%d|%ls",
                      brand_code,
                      week_number,
                      BuildOmahaExperimentDateString().c_str());

  int successful_writes = 0;
  for (int i = 0; i < arraysize(kExperimentAppGuids); ++i) {
    string16 experiment_path(google_update::kRegPathClientState);
    experiment_path += L"\\";
    experiment_path += kExperimentAppGuids[i];

    RegKey client_state(registry_hive, experiment_path.c_str(),
                        KEY_SET_VALUE);
    if (client_state.Valid()) {
      if (client_state.WriteValue(kExperimentLabels,
                                  experiment_label.c_str()) == ERROR_SUCCESS) {
        successful_writes++;
      }
    }
  }

  return (successful_writes == arraysize(kExperimentAppGuids));
}
