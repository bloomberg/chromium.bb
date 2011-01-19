// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/rlz/rlz.h"

#include "base/path_service.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

namespace {

// Gets rid of registry leftovers from testing. Returns false if there
// is nothing to clean.
bool CleanValue(const wchar_t* key_name, const wchar_t* value) {
  RegKey key;
  if (key.Open(HKEY_CURRENT_USER, key_name, KEY_READ | KEY_WRITE) !=
      ERROR_SUCCESS)
    return false;
  EXPECT_EQ(ERROR_SUCCESS, key.DeleteValue(value));
  return true;
}

// The chrome events RLZ key lives here.
const wchar_t kKeyName[] = L"Software\\Google\\Common\\Rlz\\Events\\C";

}  // namespace

TEST(RlzLibTest, RecordProductEvent) {
  DWORD recorded_value = 0;
  EXPECT_TRUE(RLZTracker::RecordProductEvent(rlz_lib::CHROME,
      rlz_lib::CHROME_OMNIBOX, rlz_lib::FIRST_SEARCH));
  const wchar_t kEvent1[] = L"C1F";
  RegKey key1;
  EXPECT_EQ(ERROR_SUCCESS, key1.Open(HKEY_CURRENT_USER, kKeyName, KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key1.ReadValueDW(kEvent1, &recorded_value));
  EXPECT_EQ(1, recorded_value);
  EXPECT_TRUE(CleanValue(kKeyName, kEvent1));

  EXPECT_TRUE(RLZTracker::RecordProductEvent(rlz_lib::CHROME,
      rlz_lib::CHROME_HOME_PAGE, rlz_lib::SET_TO_GOOGLE));
  const wchar_t kEvent2[] = L"C2S";
  RegKey key2;
  EXPECT_EQ(ERROR_SUCCESS, key2.Open(HKEY_CURRENT_USER, kKeyName, KEY_READ));
  DWORD value = 0;
  EXPECT_EQ(ERROR_SUCCESS, key2.ReadValueDW(kEvent2, &recorded_value));
  EXPECT_EQ(1, recorded_value);
  EXPECT_TRUE(CleanValue(kKeyName, kEvent2));
}

TEST(RlzLibTest, CleanProductEvents) {
  DWORD recorded_value = 0;
  EXPECT_TRUE(RLZTracker::RecordProductEvent(rlz_lib::CHROME,
      rlz_lib::CHROME_OMNIBOX, rlz_lib::FIRST_SEARCH));
  const wchar_t kEvent1[] = L"C1F";
  RegKey key1;
  EXPECT_EQ(ERROR_SUCCESS, key1.Open(HKEY_CURRENT_USER, kKeyName, KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key1.ReadValueDW(kEvent1, &recorded_value));
  EXPECT_EQ(1, recorded_value);

  EXPECT_TRUE(RLZTracker::ClearAllProductEvents(rlz_lib::CHROME));
  EXPECT_FALSE(CleanValue(kKeyName, kEvent1));
}
