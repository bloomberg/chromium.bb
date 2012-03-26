// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/metrics/field_trial.h"
#include "base/stringprintf.h"
#include "breakpad/src/client/windows/common/ipc_protocol.h"
#include "chrome/app/breakpad_field_trial_win.h"
#include "chrome/app/breakpad_win.h"
#include "chrome/common/child_process_logging.h"
#include "testing/gtest/include/gtest/gtest.h"

using breakpad_win::g_custom_entries;
using breakpad_win::g_experiment_chunks_offset;
using breakpad_win::g_num_of_experiments_offset;

class ChromeAppBreakpadTest : public testing::Test {
 public:
  ChromeAppBreakpadTest() {
    testing::InitCustomInfoEntries();
  }

 protected:
  typedef std::vector<base::FieldTrial::NameGroupId> Experiments;
  void ValidateExperimentChunks(const Experiments& experiments) {
    UpdateExperiments();
    EXPECT_STREQ(base::StringPrintf(L"%d", experiments.size()).c_str(),
                 (*g_custom_entries)[g_num_of_experiments_offset].value);
    // We make a copy of the array that we empty as we find the experiments.
    Experiments experiments_left(experiments);
    for (int i = 0; i < kMaxReportedExperimentChunks; ++i) {
      EXPECT_STREQ(base::StringPrintf(L"experiment-chunk-%i", i + 1).c_str(),
                   (*g_custom_entries)[g_experiment_chunks_offset + i].name);
      if (experiments_left.empty()) {
        // All other slots should be empty.
        EXPECT_STREQ(
            L"", (*g_custom_entries)[g_experiment_chunks_offset + i].value);
      } else {
        // We can't guarantee the order, so we must search for them all.
        Experiments::const_iterator experiment = experiments_left.begin();
        while (experiment != experiments_left.end()) {
          if (wcsstr((*g_custom_entries)[g_experiment_chunks_offset + i].value,
              base::StringPrintf(
                  L"%x-%x", experiment->name, experiment->group).c_str())) {
            experiment = experiments_left.erase(experiment);
          } else {
            ++experiment;
          }
        }
      }
    }
    EXPECT_TRUE(experiments_left.empty());
  }

 private:
  static const wchar_t* kNumExperiments;
  static const size_t kNumExperimentsSize;
};

const wchar_t* ChromeAppBreakpadTest::kNumExperiments = L"num-experiments";
const size_t ChromeAppBreakpadTest::kNumExperimentsSize =
    wcslen(ChromeAppBreakpadTest::kNumExperiments);

TEST_F(ChromeAppBreakpadTest, ExperimentList) {
  base::FieldTrialList field_trial_list("ABCDE");
  base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial("All", "To");
  base::FieldTrial::NameGroupId name_group_id;
  trial->GetNameGroupId(&name_group_id);
  Experiments experiments;
  experiments.push_back(name_group_id);
  ValidateExperimentChunks(experiments);

  trial = base::FieldTrialList::CreateFieldTrial("There", "You Are");
  trial->GetNameGroupId(&name_group_id);
  experiments.push_back(name_group_id);
  ValidateExperimentChunks(experiments);

  trial = base::FieldTrialList::CreateFieldTrial("Peter", "Sellers");
  trial->GetNameGroupId(&name_group_id);
  experiments.push_back(name_group_id);
  ValidateExperimentChunks(experiments);

  trial = base::FieldTrialList::CreateFieldTrial("Eat me", "Drink me");
  trial->GetNameGroupId(&name_group_id);
  experiments.push_back(name_group_id);
  ValidateExperimentChunks(experiments);
}
