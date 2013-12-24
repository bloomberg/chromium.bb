// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/validation_message_bubble_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

gfx::Size GetSizeForMessages(const std::string& main_text,
                             const std::string& sub_text) {
  ValidationMessageBubbleDelegate delegate(
      gfx::Rect(), base::UTF8ToUTF16(main_text),
      base::UTF8ToUTF16(sub_text), NULL);
  return delegate.GetPreferredSize();
}

TEST(ValidationMessageBubbleDelegate, Size) {
  gfx::Size short_main_empty_sub_size = GetSizeForMessages("foo", "");
  EXPECT_LE(ValidationMessageBubbleDelegate::kWindowMinWidth,
            short_main_empty_sub_size.width());
  EXPECT_LE(0, short_main_empty_sub_size.height());

  gfx::Size long_main_empty_sub_size = GetSizeForMessages(
      "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod"
      " tempor incididunt ut labore et dolore magna aliqua.", "");
  EXPECT_GE(ValidationMessageBubbleDelegate::kWindowMaxWidth,
            long_main_empty_sub_size.width());
  EXPECT_GT(long_main_empty_sub_size.height(),
            short_main_empty_sub_size.height());

  gfx::Size short_main_medium_sub_size =
      GetSizeForMessages("foo", "foo bar baz");
  EXPECT_GT(short_main_medium_sub_size.width(),
            short_main_empty_sub_size.width());
  EXPECT_GT(short_main_medium_sub_size.height(),
            short_main_empty_sub_size.height());

  gfx::Size short_main_long_sub_size = GetSizeForMessages("foo",
      "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod"
      " tempor incididunt ut labore et dolore magna aliqua.");
  EXPECT_GT(short_main_long_sub_size.width(),
            short_main_medium_sub_size.width());
  EXPECT_GE(ValidationMessageBubbleDelegate::kWindowMaxWidth,
            short_main_long_sub_size.width());
  EXPECT_GT(short_main_long_sub_size.height(),
            short_main_medium_sub_size.height());
}

}
