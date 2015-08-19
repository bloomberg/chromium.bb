// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_info.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ShortcutInfoTest : public testing::Test {
 public:
  ShortcutInfoTest() : info_(GURL()) {}

 protected:
  ShortcutInfo info_;
  content::Manifest manifest_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutInfoTest);
};

TEST_F(ShortcutInfoTest, NameUpdates) {
  info_.name = base::ASCIIToUTF16("old");
  manifest_.name = base::NullableString16(base::ASCIIToUTF16("new"), false);
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.name.string(), info_.name);
}

TEST_F(ShortcutInfoTest, ShortNameUpdates) {
  info_.short_name = base::ASCIIToUTF16("old");
  manifest_.short_name =
      base::NullableString16(base::ASCIIToUTF16("new"), false);
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.short_name.string(), info_.short_name);
}

TEST_F(ShortcutInfoTest, NameFallsBackToShortName) {
  manifest_.short_name =
      base::NullableString16(base::ASCIIToUTF16("short_name"), false);
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.short_name.string(), info_.name);
}

TEST_F(ShortcutInfoTest, ShortNameFallsBackToName) {
  manifest_.name = base::NullableString16(base::ASCIIToUTF16("name"), false);
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.name.string(), info_.short_name);
}

TEST_F(ShortcutInfoTest, UserTitleBecomesShortName) {
  manifest_.short_name =
      base::NullableString16(base::ASCIIToUTF16("name"), false);
  info_.user_title = base::ASCIIToUTF16("title");
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.short_name.string(), info_.user_title);
}
