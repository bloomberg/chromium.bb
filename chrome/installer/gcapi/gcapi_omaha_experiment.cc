// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi/gcapi_omaha_experiment.h"

#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/installer/gcapi/gcapi.h"
#include "chrome/installer/util/google_update_experiment_util.h"
#include "chrome/installer/util/google_update_settings.h"

using base::Time;
using base::TimeDelta;

namespace {

// Returns the number of weeks since 2/3/2003.
int GetCurrentRlzWeek() {
  Time::Exploded february_third_2003_exploded = {2003, 2, 1, 3, 0, 0, 0, 0};
  Time f = Time::FromUTCExploded(february_third_2003_exploded);
  TimeDelta delta = Time::Now() - f;
  return delta.InDays() / 7;
}

}  // namespace

bool SetReactivationExperimentLabels(const wchar_t* brand_code,
                                     int shell_mode) {
  if (!brand_code) {
    return false;
  }

  int week_number = GetCurrentRlzWeek();
  if (week_number < 0 || week_number > 999)
    week_number = 999;

  string16 experiment_labels;
  base::SStringPrintf(&experiment_labels,
                      L"reacbrand=%ls_%d|%ls",
                      brand_code,
                      week_number,
                      installer::BuildExperimentDateString().c_str());

  return GoogleUpdateSettings::SetExperimentLabels(
      shell_mode == GCAPI_INVOKED_UAC_ELEVATION,
      experiment_labels);
}
