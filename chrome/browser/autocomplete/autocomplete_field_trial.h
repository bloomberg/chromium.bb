// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_FIELD_TRIAL_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_FIELD_TRIAL_H_

#include <string>

#include "base/basictypes.h"

// This class manages the Autocomplete field trials.
class AutocompleteFieldTrial {
 public:
  // Creates the field trial groups.
  // *** MUST NOT BE CALLED MORE THAN ONCE. ***
  static void Activate();

  // ---------------------------------------------------------
  // For the inline History Quick Provider field trial.

  // Returns whether the user is in any field trial group for this
  // field trial.  False indicates that the field trial wasn't
  // successfully created for some reason.
  static bool InDisallowInlineHQPFieldTrial();

  // Returns whether the user should get the experiment setup or
  // the default setup for this field trial.  The experiment
  // group prohibits inlining suggestions.
  static bool InDisallowInlineHQPFieldTrialExperimentGroup();

  // ---------------------------------------------------------
  // For the History Quick Provider new scoring field trial.

  // Returns whether the user is in any field trial group for this
  // field trial.  False indicates that the field trial wasn't
  // successfully created for some reason.
  static bool InHQPNewScoringFieldTrial();

  // Returns whether the user should get the experimental setup or
  // the default setup for this field trial.  The experiment
  // group uses "new scoring" (a complex multiplicative calculation
  // that, among other differences from "old scoring", uses word
  // break information).
  static bool InHQPNewScoringFieldTrialExperimentGroup();

  // ---------------------------------------------------------
  // For the HistoryURL provider disable culling redirects field trial.

  // Returns whether the user is in any group for this field trial.
  // (Should always be true unless initialization went wrong.)
  static bool InHUPCullRedirectsFieldTrial();

  // Returns whether we should disable culling of redirects in
  // HistoryURL provider.
  static bool InHUPCullRedirectsFieldTrialExperimentGroup();

  // ---------------------------------------------------------
  // For the HistoryURL provider disable creating a shorter match
  // field trial.

  // Returns whether the user is in any group for this field trial.
  // (Should always be true unless initialization went wrong.)
  static bool InHUPCreateShorterMatchFieldTrial();

  // Returns whether we should disable creating a shorter match in
  // HistoryURL provider.
  static bool InHUPCreateShorterMatchFieldTrialExperimentGroup();

  // ---------------------------------------------------------
  // For the HistoryQuick provider replace HistoryURL provider field trial.

  // Returns whether the user is in any field trial group for this
  // field trial.  False indicates that the field trial wasn't
  // successfully created for some reason.
  static bool InHQPReplaceHUPScoringFieldTrial();

  // Returns whether the user should get the experimental setup or the
  // default setup for this field trial.  The experiment group
  // simultaneously disables HistoryURL provider from searching the
  // URL database and directs HistoryQuick provider to calculate both
  // HUP-style and HQP-style scores for matches, then return whichever
  // is larger.
  static bool InHQPReplaceHUPScoringFieldTrialExperimentGroup();

  // ---------------------------------------------------------
  // For the HistoryQuick provider only count matches at word boundaries
  // field trial.

  // Returns whether the user is in any group for this field trial.
  // (Should always be true unless initialization went wrong.)
  static bool InHQPOnlyCountMatchesAtWordBoundariesFieldTrial();

  // Returns whether we should ignore all mid-word matches in
  // HistoryQuick provider.
  static bool InHQPOnlyCountMatchesAtWordBoundariesFieldTrialExperimentGroup();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AutocompleteFieldTrial);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_FIELD_TRIAL_H_
