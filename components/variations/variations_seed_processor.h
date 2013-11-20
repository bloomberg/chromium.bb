// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/metrics/field_trial.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"

namespace chrome_variations {

class ProcessedStudy;

// Helper class to instantiate field trials from a variations seed.
class VariationsSeedProcessor {
 public:
  VariationsSeedProcessor();
  virtual ~VariationsSeedProcessor();

  // Creates field trials from the specified variations |seed|, based on the
  // specified configuration (locale, current date, version and channel).
  void CreateTrialsFromSeed(const VariationsSeed& seed,
                            const std::string& locale,
                            const base::Time& reference_date,
                            const base::Version& version,
                            Study_Channel channel,
                            Study_FormFactor form_factor);

 private:
  friend class VariationsSeedProcessorTest;
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           AllowForceGroupAndVariationId);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           AllowVariationIdWithForcingFlag);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, CheckStudyChannel);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, CheckStudyFormFactor);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, CheckStudyLocale);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, CheckStudyPlatform);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, CheckStudyStartDate);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, CheckStudyVersion);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           FilterAndValidateStudies);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           ForbidForceGroupWithVariationId);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, ForceGroupWithFlag1);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, ForceGroupWithFlag2);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           ForceGroup_ChooseFirstGroupWithFlag);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           ForceGroup_DontChooseGroupWithFlag);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, IsStudyExpired);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, ValidateStudy);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, VariationParams);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           VariationParamsWithForcingFlag);

  // Check if the |study| is only associated with platform Android/iOS and
  // channel dev/canary. If so, forcing flag and variation id can both be set.
  // (Otherwise, forcing_flag and variation_id are mutually exclusive.)
  bool AllowVariationIdWithForcingFlag(const Study& study);

  // Filters the list of studies in |seed| and validates and pre-processes them,
  // adding any kept studies to |filtered_studies| list. Ensures that the
  // resulting list will not have more than one study with the same name.
  void FilterAndValidateStudies(const VariationsSeed& seed,
                                const std::string& locale,
                                const base::Time& reference_date,
                                const base::Version& version,
                                Study_Channel channel,
                                Study_FormFactor form_factor,
                                std::vector<ProcessedStudy>* filtered_studies);

  // Validates |study| and if valid, adds it to |filtered_studies| as a
  // ProcessedStudy object.
  void ValidateAndAddStudy(const Study& study,
                           bool is_expired,
                           std::vector<ProcessedStudy>* filtered_studies);

  // Checks whether a study is applicable for the given |channel| per |filter|.
  bool CheckStudyChannel(const Study_Filter& filter, Study_Channel channel);

  // Checks whether a study is applicable for the given |form_factor| per
  // |filter|.
  bool CheckStudyFormFactor(const Study_Filter& filter,
                            Study_FormFactor form_factor);

  // Checks whether a study is applicable for the given |locale| per |filter|.
  bool CheckStudyLocale(const Study_Filter& filter, const std::string& locale);

  // Checks whether a study is applicable for the given |platform| per |filter|.
  bool CheckStudyPlatform(const Study_Filter& filter, Study_Platform platform);

  // Checks whether a study is applicable for the given date/time per |filter|.
  bool CheckStudyStartDate(const Study_Filter& filter,
                           const base::Time& date_time);

  // Checks whether a study is applicable for the given version per |filter|.
  bool CheckStudyVersion(const Study_Filter& filter,
                         const base::Version& version);

  // Creates and registers a field trial from the |processed_study| data.
  // Disables the trial if |processed_study.is_expired| is true.
  void CreateTrialFromStudy(const ProcessedStudy& processed_study);

  // Checks whether |study| is expired using the given date/time.
  bool IsStudyExpired(const Study& study, const base::Time& date_time);

  // Returns whether |study| should be disabled according to its restriction
  // parameters. Uses |version_info| for min / max version checks,
  // |reference_date| for the start date check and |channel| for channel
  // checks.
  bool ShouldAddStudy(const Study& study,
                      const std::string& locale,
                      const base::Time& reference_date,
                      const base::Version& version,
                      Study_Channel channel,
                      Study_FormFactor form_factor);

  // Validates the sanity of |study| and computes the total probability.
  bool ValidateStudyAndComputeTotalProbability(
      const Study& study,
      base::FieldTrial::Probability* total_probability);

  DISALLOW_COPY_AND_ASSIGN(VariationsSeedProcessor);
};

}  // namespace chrome_variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_
