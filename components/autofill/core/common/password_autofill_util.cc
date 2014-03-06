// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_autofill_util.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/autofill/core/common/autofill_switches.h"

namespace autofill {

namespace {

const char kDisableIgnoreAutocompleteOffFieldTrialName[] =
    "DisableIgnoreAutocompleteOff";
const char kEnablingGroup[] = "ENABLED";

bool InDisableIgnoreAutocompleteOffGroup() {
  std::string group_name = base::FieldTrialList::FindFullName(
      kDisableIgnoreAutocompleteOffFieldTrialName);

  return group_name.compare(kEnablingGroup) == 0;
}

}  // namespace

// We ignore autocomplete='off' unless the user has specified the command line
// flag instructing otherwise or is in the field trial group specifying that
// ignore autocomplete='off' should be disabled.
bool ShouldIgnoreAutocompleteOffForPasswordFields() {
  // TODO(jww): The field trial is scheduled to end 2014/9/1. At latest, we
  // should remove the field trial and switch by then.
  return !InDisableIgnoreAutocompleteOffGroup() &&
         !CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableIgnoreAutocompleteOff);
}

}  // namespace autofill
