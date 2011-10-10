// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings_temp_storage.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <vector>

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/test/base/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class SignedSettingsTempStorageTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ref_map_["some_stuff"] = "a=35;code=64";
    ref_map_["another_stuff"] = "";
    ref_map_["name"] = "value";
    ref_map_["2bc6aa16-e0ea-11df-b13d-18a90520e2e5"] = "512";

    SignedSettingsTempStorage::RegisterPrefs(&local_state_);
  }

  std::map<std::string, std::string> ref_map_;
  TestingPrefService local_state_;
};

TEST_F(SignedSettingsTempStorageTest, Basic) {
  typedef std::map<std::string, std::string>::iterator It;
  for (It it = ref_map_.begin(); it != ref_map_.end(); ++it) {
    EXPECT_TRUE(SignedSettingsTempStorage::Store(it->first,
                                                 it->second,
                                                 &local_state_));
  }
  for (It it = ref_map_.begin(); it != ref_map_.end(); ++it) {
    std::string value;
    EXPECT_TRUE(SignedSettingsTempStorage::Retrieve(it->first, &value,
                                                    &local_state_));
    EXPECT_EQ(it->second, value);
  }
  std::string value;
  EXPECT_FALSE(SignedSettingsTempStorage::Retrieve("non-existent tv-series",
                                                   &value,
                                                   &local_state_));
}

}  // namespace chromeos
