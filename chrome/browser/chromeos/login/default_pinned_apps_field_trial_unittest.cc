// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/default_pinned_apps_field_trial.h"

#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/common/metrics/entropy_provider.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class DefaultPinnedAppsFieldTrialTest : public testing::Test {
 public:
  DefaultPinnedAppsFieldTrialTest() {}
  virtual ~DefaultPinnedAppsFieldTrialTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    CreateFieldTrialList();
    CreateLocalState();
  }
  virtual void TearDown() OVERRIDE {
    Reset();
  }

  void CreateFieldTrialList() {
    if (trial_list_)
      trial_list_.reset();
    trial_list_.reset(new base::FieldTrialList(
        new metrics::SHA1EntropyProvider("client_id")));
    default_pinned_apps_field_trial::SetupTrial();
  }

  void CreateLocalState() {
    local_state_.reset(new TestingPrefServiceSimple);
    default_pinned_apps_field_trial::RegisterPrefs(local_state_->registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(local_state_.get());
  }

  void Reset() {
    TestingBrowserProcess::GetGlobal()->SetLocalState(NULL);
    local_state_.reset();
    trial_list_.reset();
    default_pinned_apps_field_trial::ResetStateForTest();
  }

 private:
  scoped_ptr<base::FieldTrialList> trial_list_;
  scoped_ptr<TestingPrefServiceSimple> local_state_;

  DISALLOW_COPY_AND_ASSIGN(DefaultPinnedAppsFieldTrialTest);
};

// Tests existing user gets to "Existing" group.
TEST_F(DefaultPinnedAppsFieldTrialTest, ExistingUser) {
  default_pinned_apps_field_trial::SetupForUser("existing@gmail.com",
                                                false /* is_new_user */);
  base::FieldTrial* trial = base::FieldTrialList::Find("DefaultPinnedApps");
  ASSERT_TRUE(trial != NULL);
  EXPECT_EQ("Existing", trial->group_name());
}

// Tests new user gets to either "Control" or "Alternate group. And the user
// should get to the same group all the time.
TEST_F(DefaultPinnedAppsFieldTrialTest, NewUser) {
  const char* kTestUser = "new@gmail.com";
  default_pinned_apps_field_trial::SetupForUser(kTestUser,
                                                true /* is_new_user */);
  base::FieldTrial* trial = base::FieldTrialList::Find("DefaultPinnedApps");
  ASSERT_TRUE(trial != NULL);
  const std::string allocated_group = trial->group_name();
  EXPECT_TRUE("Control" == allocated_group || "Alternate" == allocated_group);

  // Pretend user signs out and signs back in.
  default_pinned_apps_field_trial::ResetStateForTest();
  CreateFieldTrialList();

  // This time, the user is no longer a new user.
  default_pinned_apps_field_trial::SetupForUser(kTestUser,
                                                false /* is_new_user */);

  // But he/she should still be included in the trial and in the same group.
  trial = base::FieldTrialList::Find("DefaultPinnedApps");
  EXPECT_EQ(allocated_group, trial->group_name());
}

}  // namespace chromeos
