// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_FIELD_TRIAL_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_FIELD_TRIAL_H_
#pragma once

#include <string>

#include "base/basictypes.h"

// This class manages the Autocomplete field trial.
class AutocompleteFieldTrial {
 public:
  // Creates the field trial(s) groups.
  // *** MUST NOT BE CALLED MORE THAN ONCE. ***
  static void Activate();

  // ---------------------------------------------------------
  // For the aggressive History URL Provider field trial.

  // Returns whether the user is in any field trial group for this
  // field trial.  False indicates that the field trial wasn't
  // successfully created for some reason.
  static bool InAggressiveHUPFieldTrial();

  // Returns whether the user should get the experiment setup or
  // the default setup for this field trial.
  static bool InAggressiveHUPFieldTrialExperimentGroup();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AutocompleteFieldTrial);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_FIELD_TRIAL_H_
