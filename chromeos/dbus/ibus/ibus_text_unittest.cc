// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(nona): Add more tests.

#include "chromeos/dbus/ibus/ibus_text.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/dbus/ibus/ibus_object.h"
#include "dbus/message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(IBusTextTest, WriteReadTest) {
  const char kSampleText[] = "Sample Text";
  const IBusText::UnderlineAttribute kSampleUnderlineAttribute1 = {
    IBusText::IBUS_TEXT_UNDERLINE_SINGLE, 10, 20};

  const IBusText::UnderlineAttribute kSampleUnderlineAttribute2 = {
    IBusText::IBUS_TEXT_UNDERLINE_DOUBLE, 11, 21};

  const IBusText::UnderlineAttribute kSampleUnderlineAttribute3 = {
    IBusText::IBUS_TEXT_UNDERLINE_ERROR, 12, 22};

  const IBusText::SelectionAttribute kSampleSelectionAttribute = {30, 40};

  // Make IBusText
  IBusText text;
  text.set_text(kSampleText);
  std::vector<IBusText::UnderlineAttribute>* underline_attributes =
      text.mutable_underline_attributes();
  underline_attributes->push_back(kSampleUnderlineAttribute1);
  underline_attributes->push_back(kSampleUnderlineAttribute2);
  underline_attributes->push_back(kSampleUnderlineAttribute3);
  std::vector<IBusText::SelectionAttribute>* selection_attributes =
      text.mutable_selection_attributes();
  selection_attributes->push_back(kSampleSelectionAttribute);

  // Write to Response object.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  AppendIBusText(text, &writer);

  // Read from Response object.
  dbus::MessageReader reader(response.get());
  IBusText expected_text;
  ASSERT_TRUE(PopIBusText(&reader, &expected_text));
  EXPECT_EQ(expected_text.text(), kSampleText);
  EXPECT_EQ(3U, expected_text.underline_attributes().size());
  EXPECT_EQ(1U, expected_text.selection_attributes().size());
}

}  // namespace chromeos
