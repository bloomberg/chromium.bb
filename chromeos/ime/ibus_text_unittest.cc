// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(nona): Add more tests.

#include "chromeos/ime/ibus_text.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(IBusTextTest, CopyTest) {
  const char kSampleText[] = "Sample Text";
  const IBusText::UnderlineAttribute kSampleUnderlineAttribute1 = {
    IBusText::IBUS_TEXT_UNDERLINE_SINGLE, 10, 20};

  const IBusText::UnderlineAttribute kSampleUnderlineAttribute2 = {
    IBusText::IBUS_TEXT_UNDERLINE_DOUBLE, 11, 21};

  const IBusText::UnderlineAttribute kSampleUnderlineAttribute3 = {
    IBusText::IBUS_TEXT_UNDERLINE_ERROR, 12, 22};

  // Make IBusText
  IBusText text;
  text.set_text(kSampleText);
  std::vector<IBusText::UnderlineAttribute>* underline_attributes =
      text.mutable_underline_attributes();
  underline_attributes->push_back(kSampleUnderlineAttribute1);
  underline_attributes->push_back(kSampleUnderlineAttribute2);
  underline_attributes->push_back(kSampleUnderlineAttribute3);
  text.set_selection_start(30);
  text.set_selection_end(40);

  IBusText text2;
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
