// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_autofill_util.h"

#include "base/command_line.h"
#include "components/autofill/core/common/autofill_switches.h"

namespace autofill {

// We ignore autocomplete='off' if the user has specified the command line
// feature to enable it.
bool ShouldIgnoreAutocompleteOffForPasswordFields() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableIgnoreAutocompleteOff);
}

}  // namespace autofill
