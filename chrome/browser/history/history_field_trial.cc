// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_field_trial.h"

#include "base/metrics/field_trial.h"

namespace history {

namespace {

int g_low_mem_trial_group = 0;
bool g_low_mem_trial = false;

static const char kHistoryFieldTrialName[] = "History";

}  // namespace

// static
void HistoryFieldTrial::Activate() {
  if (g_low_mem_trial_group)
    return;  // Already initialized.

  scoped_refptr<base::FieldTrial> trial(new base::FieldTrial(
      kHistoryFieldTrialName, 1000, "Inactive", 2012, 2, 1));

  g_low_mem_trial_group = trial->AppendGroup("LowMem", 50);  // 5%
  if (trial->group() == g_low_mem_trial_group)
    g_low_mem_trial = true;
}

// static
bool HistoryFieldTrial::IsLowMemFieldTrial() {
  return g_low_mem_trial;
}

// static
std::string HistoryFieldTrial::GetGroupSuffix() {
  return g_low_mem_trial ? std::string("_LowMem") : std::string();
}

}  // namespace history
