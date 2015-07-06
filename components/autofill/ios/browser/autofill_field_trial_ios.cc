// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/browser/autofill_field_trial_ios.h"

#include "base/command_line.h"
#include "components/autofill/core/common/autofill_switches.h"

namespace autofill {

bool AutofillFieldTrialIOS::IsFullFormAutofillEnabled() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(autofill::switches::kDisableFullFormAutofillIOS))
    return false;
  if (command_line->HasSwitch(autofill::switches::kEnableFullFormAutofillIOS))
    return true;

  // TODO(bondd): Control via a FieldTrial.
  return false;
}

}  // namespace autofill
