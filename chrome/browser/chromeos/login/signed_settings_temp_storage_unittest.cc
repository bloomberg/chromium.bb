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
    ref_map_.Set("some_stuff", base::Value::CreateStringValue("a=35;code=64"));
    ref_map_.Set("another_stuff", base::Value::CreateBooleanValue(false));
    ref_map_.Set("name", base::Value::CreateStringValue("value"));
    ref_map_.Set("2bc6aa16-e0ea-11df-b13d-18a90520e2e5",
                 base::Value::CreateIntegerValue(512));

    SignedSettingsTempStorage::RegisterPrefs(&local_state_);
  }

  base::DictionaryValue ref_map_;
  TestingPrefService local_state_;
};

TEST_F(SignedSettingsTempStorageTest, Basic) {
  typedef base::DictionaryValue::key_iterator It;
  base::Value* value;
  for (It it = ref_map_.begin_keys(); it != ref_map_.end_keys(); ++it) {
    ref_map_.Get(*it, &value);
    EXPECT_TRUE(SignedSettingsTempStorage::Store(*it, *value,
                                                 &local_state_));
  }
  for (It it = ref_map_.begin_keys(); it != ref_map_.end_keys(); ++it) {
    EXPECT_TRUE(SignedSettingsTempStorage::Retrieve(*it, &value,
                                                    &local_state_));
    base::Value* orignal_value;
    ref_map_.Get(*it, &orignal_value);
    EXPECT_TRUE(orignal_value->Equals(value));
  }
  EXPECT_FALSE(SignedSettingsTempStorage::Retrieve("non-existent tv-series",
                                                   &value,
                                                   &local_state_));
}

}  // namespace chromeos
