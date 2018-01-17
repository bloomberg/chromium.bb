// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/vr/test/mock_render_text.h"
#include "testing/gtest/include/gtest/gtest.h"

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

TEST(Text, Formatting) {
  TextFormatting formatting;
  formatting.push_back(
      TextFormattingAttribute(SK_ColorGREEN, gfx::Range(1, 2)));
  formatting.push_back(
      TextFormattingAttribute(gfx::Font::Weight::BOLD, gfx::Range(3, 4)));
  formatting.push_back(
      TextFormattingAttribute(gfx::DirectionalityMode::DIRECTIONALITY_AS_URL));

  testing::InSequence in_sequence;
  testing::StrictMock<MockRenderText> render_text;
  EXPECT_CALL(render_text, ApplyColor(SK_ColorGREEN, gfx::Range(1, 2)));
  EXPECT_CALL(render_text,
              ApplyWeight(gfx::Font::Weight::BOLD, gfx::Range(3, 4)));
  EXPECT_CALL(render_text, SetDirectionalityMode(
                               gfx::DirectionalityMode::DIRECTIONALITY_AS_URL));

  for (const auto& attribute : formatting) {
    attribute.Apply(&render_text);
  }
}

}  // namespace vr
