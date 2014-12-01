// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_url_collection_experiment.h"

#include "base/files/scoped_temp_dir.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PasswordManagerUrlsCollectionExperimentTest : public testing::Test {
 public:
  PrefService* prefs() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
};

TEST_F(PasswordManagerUrlsCollectionExperimentTest, TestDefault) {
  EXPECT_FALSE(
      password_manager::urls_collection_experiment::ShouldShowBubble(prefs()));
}

}  // namespace
