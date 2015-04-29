// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/experimental_flags.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "components/variations/variations_associated_data.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ExperimentalFlagsTest : public ::testing::Test {
 protected:
  ExperimentalFlagsTest() {}

  void SetUp() override {
    // Store the command line arguments to restore them at TearDown.
    argv_ = base::CommandLine::ForCurrentProcess()->argv();
  }

  void TearDown() override {
    // Reset command line.
    base::CommandLine::Reset();
    base::CommandLine::Init(0, NULL);
    base::CommandLine::ForCurrentProcess()->InitFromArgv(argv_);

    // Reset Finch.
    variations::testing::ClearAllVariationParams();
  }

 private:
  base::CommandLine::StringVector argv_;

  DISALLOW_COPY_AND_ASSIGN(ExperimentalFlagsTest);
};

TEST_F(ExperimentalFlagsTest, MemoryWedgeSwitchNoFinch) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kIOSMemoryWedgeSize, "10");

  EXPECT_EQ(10u, experimental_flags::MemoryWedgeSizeInMB());
}

TEST_F(ExperimentalFlagsTest, MemoryWedgeNoSwitchFinchNoParam) {
  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial("MemoryWedge", "Default");

  EXPECT_EQ(0u, experimental_flags::MemoryWedgeSizeInMB());
}

TEST_F(ExperimentalFlagsTest, MemoryWedgeNoSwitchFinchParam) {
  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial("MemoryWedge", "Experiment2");
  std::map<std::string, std::string> params;
  params[std::string("wedge_size")] = "20";
  ASSERT_TRUE(variations::AssociateVariationParams("MemoryWedge", "Experiment2",
                                                   params));

  EXPECT_EQ(20u, experimental_flags::MemoryWedgeSizeInMB());
}

TEST_F(ExperimentalFlagsTest, MemoryWedgeNoSwitchFinchWrongParam) {
  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial("MemoryWedge", "Experiment2");
  std::map<std::string, std::string> params;
  params[std::string("wedge_size")] = "twenty";
  ASSERT_TRUE(variations::AssociateVariationParams("MemoryWedge", "Experiment2",
                                                   params));

  EXPECT_EQ(0u, experimental_flags::MemoryWedgeSizeInMB());
}

TEST_F(ExperimentalFlagsTest, MemoryWedgeSwitchFinch) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kIOSMemoryWedgeSize, "10");

  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial("MemoryWedge", "Experiment2");
  std::map<std::string, std::string> params;
  params[std::string("wedge_size")] = "20";
  ASSERT_TRUE(variations::AssociateVariationParams("MemoryWedge", "Experiment2",
                                                   params));

  EXPECT_EQ(10u, experimental_flags::MemoryWedgeSizeInMB());
}

TEST_F(ExperimentalFlagsTest, MemoryWedgeNoSwitchNoFinch) {
  EXPECT_EQ(0u, experimental_flags::MemoryWedgeSizeInMB());
}

}  // namespace
