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
#include "base/memory/scoped_temp_dir.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/logging_chrome.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class SignedSettingsTempStorageTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    ref_map_["some_stuff"] = "a=35;code=64";
    ref_map_["another_stuff"] = "";
    ref_map_["name"] = "value";
    ref_map_["2bc6aa16-e0ea-11df-b13d-18a90520e2e5"] = "512";

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    FilePath temp_file;
    ASSERT_TRUE(
        file_util::CreateTemporaryFileInDir(temp_dir_.path(), &temp_file));
    local_state_.reset(PrefService::CreatePrefService(temp_file, NULL, NULL));
    ASSERT_TRUE(NULL != local_state_.get());
    SignedSettingsTempStorage::RegisterPrefs(local_state_.get());
  }

  std::map<std::string, std::string> ref_map_;
  ScopedTempDir temp_dir_;
  scoped_ptr<PrefService> local_state_;
};

TEST_F(SignedSettingsTempStorageTest, Basic) {
  EXPECT_GT(ref_map_.size(), 3u);  // Number above 3 is many.
  typedef std::map<std::string, std::string>::iterator It;
  std::vector<It> a_list;
  for (It it = ref_map_.begin(); it != ref_map_.end(); ++it) {
    a_list.push_back(it);
  }
  std::random_shuffle(a_list.begin(), a_list.end());
  std::vector<It> b_list(a_list);
  std::copy(b_list.begin(),
            b_list.begin() + b_list.size() / 2,
            std::back_inserter(a_list));
  std::random_shuffle(a_list.begin(), a_list.end());
  for (size_t i = 0; i < a_list.size(); ++i) {
    EXPECT_TRUE(SignedSettingsTempStorage::Store(a_list[i]->first,
                                                 a_list[i]->second,
                                                 local_state_.get()));
  }
  for (int i = 0; i < 3; ++i) {
    std::copy(a_list.begin(), a_list.end(), std::back_inserter(b_list));
  }
  std::random_shuffle(b_list.begin(), b_list.end());
  std::string value;
  for (size_t i = 0; i < b_list.size(); ++i) {
    EXPECT_TRUE(SignedSettingsTempStorage::Retrieve(b_list[i]->first, &value,
                                                    local_state_.get()));
    EXPECT_EQ(b_list[i]->second, value);
    EXPECT_FALSE(SignedSettingsTempStorage::Retrieve("non-existent tv-series",
                                                     &value,
                                                     local_state_.get()));
  }
}

}  // namespace chromeos
