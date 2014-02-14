// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(nona): Add more tests.

#include "chromeos/ime/composition_text.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(CompositionTextTest, CopyTest) {
  const base::string16 kSampleText = base::UTF8ToUTF16("Sample Text");
  const CompositionText::UnderlineAttribute kSampleUnderlineAttribute1 = {
    CompositionText::COMPOSITION_TEXT_UNDERLINE_SINGLE, 10, 20};

  const CompositionText::UnderlineAttribute kSampleUnderlineAttribute2 = {
    CompositionText::COMPOSITION_TEXT_UNDERLINE_DOUBLE, 11, 21};

  const CompositionText::UnderlineAttribute kSampleUnderlineAttribute3 = {
    CompositionText::COMPOSITION_TEXT_UNDERLINE_ERROR, 12, 22};

  // Make CompositionText
  CompositionText text;
  text.set_text(kSampleText);
  std::vector<CompositionText::UnderlineAttribute>* underline_attributes =
      text.mutable_underline_attributes();
  underline_attributes->push_back(kSampleUnderlineAttribute1);
  underline_attributes->push_back(kSampleUnderlineAttribute2);
  underline_attributes->push_back(kSampleUnderlineAttribute3);
  text.set_selection_start(30);
  text.set_selection_end(40);

  CompositionText text2;
  text2.CopyFrom(text);

  EXPECT_EQ(text.text(), text2.text());
  EXPECT_EQ(text.underline_attributes().size(),
            text2.underline_attributes().size());
  for (size_t i = 0; i < text.underline_attributes().size(); ++i) {
    EXPECT_EQ(text.underline_attributes()[i].type,
              text2.underline_attributes()[i].type);
    EXPECT_EQ(text.underline_attributes()[i].start_index,
              text2.underline_attributes()[i].start_index);
    EXPECT_EQ(text.underline_attributes()[i].end_index,
              text2.underline_attributes()[i].end_index);
  }

  EXPECT_EQ(text.selection_start(), text2.selection_start());
  EXPECT_EQ(text.selection_end(), text2.selection_end());
}

}  // namespace chromeos
