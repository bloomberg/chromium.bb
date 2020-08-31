// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_HATS_HATS_FINCH_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_HATS_HATS_FINCH_HELPER_H_

#include <limits.h>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"

class Profile;

namespace chromeos {

// Provides an API for HatsNotificationController to retrieve processed
// information related to the hats finch experiment.
class HatsFinchHelper {
 public:
  explicit HatsFinchHelper(Profile* profile);
  ~HatsFinchHelper();

  bool IsDeviceSelectedForCurrentCycle() const {
    return device_is_selected_for_cycle_;
  }

 private:
  friend class HatsFinchHelperTest;
  FRIEND_TEST_ALL_PREFIXES(HatsFinchHelperTest, InitFinchSeed_ValidValues);
  FRIEND_TEST_ALL_PREFIXES(HatsFinchHelperTest, InitFinchSeed_Invalidalues);
  FRIEND_TEST_ALL_PREFIXES(HatsFinchHelperTest, TestComputeNextDate);
  FRIEND_TEST_ALL_PREFIXES(HatsFinchHelperTest, ResetSurveyCycle);
  FRIEND_TEST_ALL_PREFIXES(HatsFinchHelperTest, ResetHats);

  static const char kProbabilityParam[];
  static const char kSurveyCycleLengthParam[];
  static const char kSurveyStartDateMsParam[];
  static const char kResetSurveyCycleParam[];
  static const char kResetAllParam[];

  // Loads all the param values from the finch seed and initializes the member
  // variables.
  void LoadFinchParamValues();

  // Returns true if the survey cycle that was active most recently has passed
  // its end date.
  bool HasPreviousCycleEnded();

  // Computes the end date of the survey cycle that is currently active.
  base::Time ComputeNextEndDate();

  // Using the params provided in the finch seed, compute and check if the
  // current device is seleted for the current survey cycle. This is computed
  // only once per survey cycle.
  void CheckForDeviceSelection();

  // The probability that the device is picked for the current suvey cycle. This
  // is provided as a param in the finch seed under the key "prob".
  double probability_of_pick_ = 0;

  // Time period in days after which the dice will be rolled again to decide if
  // the device is selected for the survey cycle. This is provided as a param
  // via the finch seed under the key "survey_cycle_length".
  int survey_cycle_length_ = INT_MAX;

  // The timestamp for the date when the experiment started with the current
  // set of parameters. This is provided as a param via the finch seed under the
  // key "survey_start_date_ms".
  base::Time first_survey_start_date_;

  // Indicates that the survey cycle needs to be reset if set to true.
  bool reset_survey_cycle_ = false;

  // If true, indicates that all hats related state on the device needs to be
  // reset.
  bool reset_hats_ = false;

  // If true, indicates that the device has been selected for the survey in the
  // current survey cycle. This is set by |CheckForDeviceSelection()|.
  bool device_is_selected_for_cycle_ = false;

  Profile* const profile_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_HATS_HATS_FINCH_HELPER_H_
