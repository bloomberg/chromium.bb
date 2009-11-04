// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/character_set_converters.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

using std::string;

class CharacterSetConverterTest : public testing::Test {
};

TEST(NameTruncation, WindowsNameTruncation) {
  using browser_sync::TrimPathStringToValidCharacter;
  PathChar array[] = {'1', '2', '\xc0', '\xe0', '3', '4', '\0'};
  PathString message = array;
  ASSERT_EQ(message.length(), arraysize(array) - 1);
  string::size_type old_length = message.length();
  while (old_length != 0) {
    TrimPathStringToValidCharacter(&message);
    if (old_length == 4)
      EXPECT_EQ(3u, message.length());
    else
      EXPECT_EQ(old_length, message.length());
    message.resize(message.length() - 1);
    old_length = message.length();
  }
  TrimPathStringToValidCharacter(&message);
}
