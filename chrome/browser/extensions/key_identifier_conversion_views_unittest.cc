// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/key_identifier_conversion_views.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/keycodes/keyboard_codes.h"

namespace {

class KeyEventFromKeyIdentifierTest : public testing::Test {
 protected:
  KeyEventFromKeyIdentifierTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
};

TEST_F(KeyEventFromKeyIdentifierTest, MatchOnIdentifier) {
  EXPECT_EQ(ui::VKEY_APPS, KeyEventFromKeyIdentifier("Apps").GetKeyCode());
  EXPECT_EQ(ui::VKEY_UNKNOWN,
      KeyEventFromKeyIdentifier("Nonsense").GetKeyCode());
}

TEST_F(KeyEventFromKeyIdentifierTest, MatchOnCharacter) {
  EXPECT_EQ(ui::VKEY_A, KeyEventFromKeyIdentifier("a").GetKeyCode());
  EXPECT_EQ(ui::VKEY_A, KeyEventFromKeyIdentifier("A").GetKeyCode());
  EXPECT_EQ(ui::VKEY_OEM_PERIOD, KeyEventFromKeyIdentifier(">").GetKeyCode());

  std::string non_printing_char(" ");
  non_printing_char[0] = static_cast<char>(1);
  EXPECT_EQ(ui::VKEY_UNKNOWN,
      KeyEventFromKeyIdentifier(non_printing_char).GetKeyCode());
}

TEST_F(KeyEventFromKeyIdentifierTest, MatchOnUnicodeCodepoint) {
  EXPECT_EQ(ui::VKEY_A, KeyEventFromKeyIdentifier("U+0041").GetKeyCode());
  EXPECT_EQ(ui::VKEY_A, KeyEventFromKeyIdentifier("U+0061").GetKeyCode());
  EXPECT_EQ(ui::VKEY_DELETE, KeyEventFromKeyIdentifier("U+007F").GetKeyCode());

  // this one exists in the map, but has no valid ui::VKEY
  EXPECT_EQ(ui::VKEY_UNKNOWN, KeyEventFromKeyIdentifier("U+030A").GetKeyCode());

  // this one is not in the map
  EXPECT_EQ(ui::VKEY_UNKNOWN, KeyEventFromKeyIdentifier("U+0001").GetKeyCode());
}

TEST_F(KeyEventFromKeyIdentifierTest, DoesNotMatchEmptyString) {
  EXPECT_EQ(ui::VKEY_UNKNOWN, KeyEventFromKeyIdentifier("").GetKeyCode());
}

TEST_F(KeyEventFromKeyIdentifierTest, ShiftModifiersAreSet) {
  EXPECT_EQ(0, KeyEventFromKeyIdentifier("1").GetFlags());

  const char* keys_with_shift[] = {
    "~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+",
    "{", "}", "|", ":", "<", ">", "?", "\""
  };
  int kNumKeysWithShift = arraysize(keys_with_shift);

  for (int i = 0; i < kNumKeysWithShift; ++i) {
    EXPECT_EQ(views::Event::EF_SHIFT_DOWN,
        KeyEventFromKeyIdentifier(keys_with_shift[i]).GetFlags());
  }
}

}  // namespace
