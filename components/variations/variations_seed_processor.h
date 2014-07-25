// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"

namespace variations {

class ProcessedStudy;

// Helper class to instantiate field trials from a variations seed.
class VariationsSeedProcessor {
 public:
  typedef base::Callback<void(uint32_t, const base::string16&)>
      UIStringOverrideCallback;

  VariationsSeedProcessor();
  virtual ~VariationsSeedProcessor();

  // Creates field trials from the specified variations |seed|, based on the
  // specified configuration (locale, current date, version, channel, form
  // factor and hardware_class).
  void CreateTrialsFromSeed(const VariationsSeed& seed,
                            const std::string& locale,
                            const base::Time& reference_date,
                            const base::Version& version,
                            Study_Channel channel,
                            Study_FormFactor form_factor,
                            const std::string& hardware_class,
                            const UIStringOverrideCallback& override_callback);

 private:
  friend class VariationsSeedProcessorTest;
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           AllowForceGroupAndVariationId);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           AllowVariationIdWithForcingFlag);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           ForbidForceGroupWithVariationId);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, ForceGroupWithFlag1);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, ForceGroupWithFlag2);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           ForceGroup_ChooseFirstGroupWithFlag);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           ForceGroup_DontChooseGroupWithFlag);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, IsStudyExpired);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, VariationParams);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           VariationParamsWithForcingFlag);

  // Check if the |study| is only associated with platform Android/iOS and
  // channel dev/canary. If so, forcing flag and variation id can both be set.
  // (Otherwise, forcing_flag and variation_id are mutually exclusive.)
  bool AllowVariationIdWithForcingFlag(const Study& study);

  // Creates and registers a field trial from the |processed_study| data.
  // Disables the trial if |processed_study.is_expired| is true.
  void CreateTrialFromStudy(const ProcessedStudy& processed_study,
                            const UIStringOverrideCallback& override_callback);

  DISALLOW_COPY_AND_ASSIGN(VariationsSeedProcessor);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_
