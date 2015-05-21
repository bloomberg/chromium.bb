// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_bubble_experiment.h"

#include "base/files/scoped_temp_dir.h"
#include "base/metrics/field_trial.h"
#include "chrome/test/base/testing_profile.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

class PasswordBubbleExperimentTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_.reset(new TestingProfile(temp_dir_.path()));

    field_trial_list_.reset(new base::FieldTrialList(
        new metrics::SHA1EntropyProvider("foo")));
  }

  void TearDown() override {
    variations::testing::ClearAllVariationParams();
  }

  PrefService* prefs() { return profile_->GetPrefs(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<base::FieldTrialList> field_trial_list_;
};

// TODO(vasilii): add tests once there is a smart bubble.
