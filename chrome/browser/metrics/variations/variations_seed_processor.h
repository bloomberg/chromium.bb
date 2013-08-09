// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/metrics/field_trial.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/metrics/proto/study.pb.h"
#include "chrome/browser/metrics/proto/trials_seed.pb.h"

namespace chrome_variations {

// Helper class to instantiate field trials from a variations seed.
class VariationsSeedProcessor {
 public:
  VariationsSeedProcessor();
  virtual ~VariationsSeedProcessor();

  // Creates field trials from the specified variations |seed|, based on the
  // specified configuration (locale, current date, version and channel).
  void CreateTrialsFromSeed(const TrialsSeed& seed,
                            const std::string& locale,
                            const base::Time& reference_date,
                            const base::Version& version,
                            Study_Channel channel);

 private:
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, CheckStudyChannel);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, CheckStudyLocale);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, CheckStudyPlatform);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, CheckStudyStartDate);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, CheckStudyVersion);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           CheckStudyVersionWildcards);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, ForceGroupWithFlag1);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, ForceGroupWithFlag2);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           ForceGroup_ChooseFirstGroupWithFlag);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           ForceGroup_DontChooseGroupWithFlag);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, IsStudyExpired);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, ValidateStudy);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, VariationParams);

  // Checks whether a study is applicable for the given |channel| per |filter|.
  bool CheckStudyChannel(const Study_Filter& filter, Study_Channel channel);

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

  // Creates and registers a field trial from the |study| data. Disables the
  // trial if |is_expired| is true.
  void CreateTrialFromStudy(const Study& study, bool is_expired);

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
                      Study_Channel channel);

  // Validates the sanity of |study| and computes the total probability.
  bool ValidateStudyAndComputeTotalProbability(
      const Study& study,
      base::FieldTrial::Probability* total_probability);

  DISALLOW_COPY_AND_ASSIGN(VariationsSeedProcessor);
};

}  // namespace chrome_variations

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_
