// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/render_text.h"

namespace vr {

TEST(Text, MultiLine) {
  const float kInitialSize = 1.0f;

  // Create an initialize a text element with a long string.
  auto text = std::make_unique<Text>(0.020);
  text->SetSize(kInitialSize, 0);
  text->SetText(base::UTF8ToUTF16(std::string(1000, 'x')));

  // Make sure we get multiple lines of rendered text from the string.
  size_t initial_num_lines = text->LayOutTextForTest().size();
  auto initial_size = text->GetTextureSizeForTest();
  EXPECT_GT(initial_num_lines, 1u);
  EXPECT_GT(initial_size.height(), 0.f);

  // Reduce the field width, and ensure that the number of lines increases along
  // with the texture height.
  text->SetSize(kInitialSize / 2, 0);
  EXPECT_GT(text->LayOutTextForTest().size(), initial_num_lines);
  EXPECT_GT(text->GetTextureSizeForTest().height(), initial_size.height());

  // Enforce single-line rendering.
  text->SetLayoutMode(kSingleLineFixedWidth);
  EXPECT_EQ(text->LayOutTextForTest().size(), 1u);
  EXPECT_LT(text->GetTextureSizeForTest().height(), initial_size.height());
}

}  // namespace vr
