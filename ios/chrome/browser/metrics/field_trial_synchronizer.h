// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_FIELD_TRIAL_SYNCHRONIZER_H_
#define IOS_CHROME_BROWSER_METRICS_FIELD_TRIAL_SYNCHRONIZER_H_

#include <string>

#include "base/macros.h"
#include "base/metrics/field_trial.h"

namespace ios {

// FieldTrialSynchronizer registers itself as an observer of FieldTrialList
// and update the list of experiment reported to in the crashes by setting
// the corresponding crash key.
class FieldTrialSynchronizer : public base::FieldTrialList::Observer {
 public:
  FieldTrialSynchronizer();
  ~FieldTrialSynchronizer() override;

 private:
  // base::FieldTrialList::Observer implementation.
  void OnFieldTrialGroupFinalized(const std::string& field_trial_name,
                                  const std::string& group_name) override;

  // Synchronizes the list of experiments into the crash keys. Called on
  // creation and when the list of field trial changes.
  void SynchronizeCrashKeyExperimentList();

  DISALLOW_COPY_AND_ASSIGN(FieldTrialSynchronizer);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_METRICS_FIELD_TRIAL_SYNCHRONIZER_H_
