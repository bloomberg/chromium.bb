// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

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
  typedef std::vector<string16> Experiments;
  void ValidateExperimentChunks(const Experiments& experiments) {
    testing::SetExperimentList(experiments);
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
                     experiment->c_str())) {
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
  Experiments experiments;
  experiments.push_back(L"ABCDE-12345");
  ValidateExperimentChunks(experiments);

  experiments.push_back(L"There-You Are");
  ValidateExperimentChunks(experiments);

  experiments.push_back(L"Peter-Sellers");
  ValidateExperimentChunks(experiments);

  experiments.push_back(L"Eat me-Drink me");
  ValidateExperimentChunks(experiments);
}
