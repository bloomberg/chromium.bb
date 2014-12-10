// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_url_collection_experiment.h"

#include "base/files/scoped_temp_dir.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/variations/entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

class PasswordManagerUrlsCollectionExperimentTest : public testing::Test {
 public:
  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kWasAllowToCollectURLBubbleShown, false);
  }

  PrefService* prefs() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
};

TEST_F(PasswordManagerUrlsCollectionExperimentTest, TestDefault) {
  EXPECT_FALSE(
      password_manager::urls_collection_experiment::ShouldShowBubble(prefs()));
}

TEST_F(PasswordManagerUrlsCollectionExperimentTest, TestMaybeShowBubbleGroup) {
  // TODO(melandory) This test case should be rewritten when decision about
  // should bubble be shown or not will be made based on Finch experiment
  // http://crbug.com/435080.
  EXPECT_FALSE(
      password_manager::urls_collection_experiment::ShouldShowBubble(prefs()));
}

TEST_F(PasswordManagerUrlsCollectionExperimentTest, TestNeverShowBubbleGroup) {
  EXPECT_FALSE(
      password_manager::urls_collection_experiment::ShouldShowBubble(prefs()));
}

TEST_F(PasswordManagerUrlsCollectionExperimentTest, TestBubbleWasAlreadyShown) {
  prefs()->SetBoolean(password_manager::prefs::kWasAllowToCollectURLBubbleShown,
                      true);
  EXPECT_FALSE(
      password_manager::urls_collection_experiment::ShouldShowBubble(prefs()));
}
