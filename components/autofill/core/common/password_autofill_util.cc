// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_autofill_util.h"

#include "base/command_line.h"
#include "components/autofill/core/common/autofill_switches.h"

namespace autofill {

// We ignore autocomplete='off' unless the user has specified the command line
// flag instructing otherwise.
bool ShouldIgnoreAutocompleteOffForPasswordFields() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableIgnoreAutocompleteOff);
}

}  // namespace autofill
