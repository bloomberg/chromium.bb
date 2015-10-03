// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads_util.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/test/mock_entropy_provider.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::FieldTrialList;

const char* kFieldTrialName = kMaterialDesignDownloadsFinchTrialName;

class MdDownloadsEnabledTest : public testing::Test {
 public:
  MdDownloadsEnabledTest() : field_trial_list_(new base::MockEntropyProvider) {}
  ~MdDownloadsEnabledTest() override {}

 private:
  base::FieldTrialList field_trial_list_;
};

TEST_F(MdDownloadsEnabledTest, DisabledByDefault) {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  ASSERT_FALSE(cl->HasSwitch(switches::kDisableMaterialDesignDownloads));
  ASSERT_FALSE(cl->HasSwitch(switches::kEnableMaterialDesignDownloads));
  ASSERT_FALSE(base::FieldTrialList::TrialExists(kFieldTrialName));
  EXPECT_FALSE(MdDownloadsEnabled());
}

TEST_F(MdDownloadsEnabledTest, EnabledByFieldTrial) {
  ASSERT_TRUE(FieldTrialList::CreateFieldTrial(kFieldTrialName, "Enabled2"));
  EXPECT_TRUE(MdDownloadsEnabled());
}

TEST_F(MdDownloadsEnabledTest, DisabledByFieldTrial) {
  ASSERT_TRUE(FieldTrialList::CreateFieldTrial(kFieldTrialName, "Disabled2"));
  EXPECT_FALSE(MdDownloadsEnabled());
}

TEST_F(MdDownloadsEnabledTest, EnabledBySwitch) {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kEnableMaterialDesignDownloads);
  EXPECT_TRUE(MdDownloadsEnabled());
}

TEST_F(MdDownloadsEnabledTest, DisabledBySwitch) {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kDisableMaterialDesignDownloads);
  EXPECT_FALSE(MdDownloadsEnabled());
}

TEST_F(MdDownloadsEnabledTest, SwitchOverridesFieldTrial1) {
  ASSERT_TRUE(FieldTrialList::CreateFieldTrial(kFieldTrialName, "Disabled2"));
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kEnableMaterialDesignDownloads);
  EXPECT_TRUE(MdDownloadsEnabled());
}

TEST_F(MdDownloadsEnabledTest, SwitchOverridesFieldTrial2) {
  ASSERT_TRUE(FieldTrialList::CreateFieldTrial(kFieldTrialName, "Enabled2"));
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kDisableMaterialDesignDownloads);
  EXPECT_FALSE(MdDownloadsEnabled());
}
