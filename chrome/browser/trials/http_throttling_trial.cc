// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/trials/http_throttling_trial.h"

#include "base/metrics/field_trial.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

void CreateHttpThrottlingTrial(PrefService* prefs) {
  DCHECK(prefs);

  // We only use one-time randomization, meaning each client will be put
  // in the same group on every startup, so that users do not have a situation
  // where they are intermittently in the experiment group, and have problems
  // because of throttling, then turn off their browser to fix the problem,
  // come back, and cannot figure out why they had problems (because throttling
  // will most likely by then be turned off).  A lesser concern is that if
  // we didn't use one-time randomization, users might notice the preference
  // in about:net-internals toggling from one state to the other.
  if (!base::FieldTrialList::IsOneTimeRandomizationEnabled())
    return;

  // Probability for each trial group (the experiment and the control) is 25%.
  const base::FieldTrial::Probability kEachGroupProbability = 25;
  const base::FieldTrial::Probability kTotalProbability = 100;
  // Disable trial a couple of days before M15 branch point.
  scoped_refptr<base::FieldTrial> trial(new base::FieldTrial(
      "HttpThrottlingEnabled", kTotalProbability, "Default", 2011, 9, 20));
  trial->UseOneTimeRandomization();

  // If the user has touched the control for whether throttling is enabled
  // or not, we only allow the Default group for the trial, and we do not
  // modify the value of prefs::kHttpThrottlingEnabled.
  if (prefs->GetBoolean(prefs::kHttpThrottlingMayExperiment)) {
    int experiment_group =
        trial->AppendGroup("Experiment", kEachGroupProbability);

    // The behavior for the control group is mostly the same as for
    // the default group, with the difference that we are guaranteed
    // that none of the users in the control group have manually
    // changed the setting (under chrome://net-internals/) for whether
    // throttling should be turned on or not.  The other point of
    // having the control group is that it's the same size as the
    // experiment group and selected the same way, so we get an
    // apples-to-apples comparison of histograms.
    trial->AppendGroup("Control", kEachGroupProbability);

    prefs->SetBoolean(prefs::kHttpThrottlingEnabled,
                      trial->group() == experiment_group);
  }
}
