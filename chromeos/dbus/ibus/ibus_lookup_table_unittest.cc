// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(nona): Add more tests.

#include "chromeos/dbus/ibus/ibus_lookup_table.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/dbus/ibus/ibus_object.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "dbus/message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
// TODO(nona): Remove ibus namespace after complete libibus removale.
namespace ibus {

TEST(IBusLookupTable, WriteReadTest) {
  const char kSampleText1[] = "Sample Text 1";
  const char kSampleText2[] = "Sample Text 2";
  const char kSampleLabel1[] = "Sample Label 1";
  const char kSampleLabel2[] = "Sample Label 2";
  const uint32 kPageSize = 11;
  const uint32 kCursorPosition = 12;
  const bool kIsCursorVisible = true;
  const IBusLookupTable::Orientation kOrientation =
      IBusLookupTable::IBUS_LOOKUP_TABLE_ORIENTATION_VERTICAL;

  // Create IBusLookupTable.
  IBusLookupTable lookup_table;
  lookup_table.set_page_size(kPageSize);
  lookup_table.set_cursor_position(kCursorPosition);
  lookup_table.set_is_cursor_visible(kIsCursorVisible);
  lookup_table.set_orientation(kOrientation);
  std::vector<IBusLookupTable::Entry>* candidates =
      lookup_table.mutable_candidates();
  IBusLookupTable::Entry entry1;
  entry1.value = kSampleText1;
  entry1.label = kSampleLabel1;
  candidates->push_back(entry1);

  IBusLookupTable::Entry entry2;
  entry2.value = kSampleText2;
  entry2.label = kSampleLabel2;
  candidates->push_back(entry2);

  // Write IBusLookupTable.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  AppendIBusLookupTable(lookup_table, &writer);

  // Read IBusLookupTable.
  IBusLookupTable target_lookup_table;
  dbus::MessageReader reader(response.get());
  PopIBusLookupTable(&reader, &target_lookup_table);

  // Check values.
  EXPECT_EQ(kPageSize, target_lookup_table.page_size());
  EXPECT_EQ(kCursorPosition, target_lookup_table.cursor_position());
  EXPECT_EQ(kIsCursorVisible, target_lookup_table.is_cursor_visible());
  EXPECT_EQ(kOrientation, target_lookup_table.orientation());
  ASSERT_EQ(2UL, target_lookup_table.candidates().size());
  EXPECT_EQ(kSampleText1, target_lookup_table.candidates().at(0).value);
  EXPECT_EQ(kSampleText2, target_lookup_table.candidates().at(1).value);
  EXPECT_EQ(kSampleLabel1, target_lookup_table.candidates().at(0).label);
  EXPECT_EQ(kSampleLabel2, target_lookup_table.candidates().at(1).label);
}

TEST(IBusLookupTable, WriteReadWithoutLableTest) {
  const char kSampleText1[] = "Sample Text 1";
  const char kSampleText2[] = "Sample Text 2";
  const uint32 kPageSize = 11;
  const uint32 kCursorPosition = 12;
  const bool kIsCursorVisible = true;
  const IBusLookupTable::Orientation kOrientation =
      IBusLookupTable::IBUS_LOOKUP_TABLE_ORIENTATION_VERTICAL;

  // Create IBusLookupTable.
  IBusLookupTable lookup_table;
  lookup_table.set_page_size(kPageSize);
  lookup_table.set_cursor_position(kCursorPosition);
  lookup_table.set_is_cursor_visible(kIsCursorVisible);
  lookup_table.set_orientation(kOrientation);
  std::vector<IBusLookupTable::Entry>* candidates =
      lookup_table.mutable_candidates();
  IBusLookupTable::Entry entry1;
  entry1.value = kSampleText1;
  candidates->push_back(entry1);

  IBusLookupTable::Entry entry2;
  entry2.value = kSampleText2;
  candidates->push_back(entry2);

  // Write IBusLookupTable.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  AppendIBusLookupTable(lookup_table, &writer);

  // Read IBusLookupTable.
  IBusLookupTable target_lookup_table;
  dbus::MessageReader reader(response.get());
  PopIBusLookupTable(&reader, &target_lookup_table);

  // Check values.
  EXPECT_EQ(kPageSize, target_lookup_table.page_size());
  EXPECT_EQ(kCursorPosition, target_lookup_table.cursor_position());
  EXPECT_EQ(kIsCursorVisible, target_lookup_table.is_cursor_visible());
  EXPECT_EQ(kOrientation, target_lookup_table.orientation());
  ASSERT_EQ(2UL, target_lookup_table.candidates().size());
  EXPECT_EQ(kSampleText1, target_lookup_table.candidates().at(0).value);
  EXPECT_EQ(kSampleText2, target_lookup_table.candidates().at(1).value);
  EXPECT_TRUE(target_lookup_table.candidates().at(0).label.empty());
  EXPECT_TRUE(target_lookup_table.candidates().at(1).label.empty());
}

TEST(IBusLookupTable, ReadMozcCandidateTest) {
  // IBusObjectWriter does not support attachment field handling, so manually
  // create IBusLookupTable with mozc.candidates attachment.
  const char kSampleText1[] = "Sample Text 1";
  const char kSampleText2[] = "Sample Text 2";
  const char kSampleLabel1[] = "Sample Label 1";
  const char kSampleLabel2[] = "Sample Label 2";
  const uint32 kPageSize = 11;
  const uint32 kCursorPosition = 12;
  const bool kIsCursorVisible = true;
  const IBusLookupTable::Orientation kOrientation =
      IBusLookupTable::IBUS_LOOKUP_TABLE_ORIENTATION_VERTICAL;
  const uint8 kSampleBuffer[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  const size_t kSampleBufferLength = 6UL;

  // Create IBusLookupTable.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter top_variant_writer(NULL);
  writer.OpenVariant("(sa{sv}uubbiavav)", &top_variant_writer);
  dbus::MessageWriter contents_writer(NULL);
  top_variant_writer.OpenStruct(&contents_writer);
  contents_writer.AppendString("IBusLookupTable");

  // Write attachment field.
  dbus::MessageWriter attachment_array_writer(NULL);
  contents_writer.OpenArray("{sv}", &attachment_array_writer);
  dbus::MessageWriter entry_writer(NULL);
  attachment_array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString("mozc.candidates");
  dbus::MessageWriter variant_writer(NULL);
  entry_writer.OpenVariant("v", &variant_writer);
  dbus::MessageWriter sub_variant_writer(NULL);
  variant_writer.OpenVariant("ay", &sub_variant_writer);
  sub_variant_writer.AppendArrayOfBytes(kSampleBuffer, kSampleBufferLength);

  // Close attachment field container.
  variant_writer.CloseContainer(&sub_variant_writer);
  entry_writer.CloseContainer(&variant_writer);
  attachment_array_writer.CloseContainer(&entry_writer);
  contents_writer.CloseContainer(&attachment_array_writer);

  // Write IBusLookupTable contents.
  contents_writer.AppendUint32(kPageSize);
  contents_writer.AppendUint32(kCursorPosition);
  contents_writer.AppendBool(kIsCursorVisible);
  contents_writer.AppendBool(false);
  contents_writer.AppendInt32(static_cast<int32>(kOrientation));

  // Write entries
  dbus::MessageWriter text_writer(NULL);
  contents_writer.OpenArray("v", &text_writer);
  AppendStringAsIBusText(kSampleText1, &text_writer);
  AppendStringAsIBusText(kSampleText2, &text_writer);
  contents_writer.CloseContainer(&text_writer);
  dbus::MessageWriter label_writer(NULL);
  contents_writer.OpenArray("v", &label_writer);
  AppendStringAsIBusText(kSampleLabel1, &label_writer);
  AppendStringAsIBusText(kSampleLabel2, &label_writer);
  contents_writer.CloseContainer(&label_writer);

  // Close all containers.
  top_variant_writer.CloseContainer(&contents_writer);
  writer.CloseContainer(&top_variant_writer);

  // Read IBusLookupTable.
  IBusLookupTable target_lookup_table;
  dbus::MessageReader reader(response.get());
  PopIBusLookupTable(&reader, &target_lookup_table);

  // Check values.
  EXPECT_EQ(kPageSize, target_lookup_table.page_size());
  EXPECT_EQ(kCursorPosition, target_lookup_table.cursor_position());
  EXPECT_EQ(kIsCursorVisible, target_lookup_table.is_cursor_visible());
  EXPECT_EQ(kOrientation, target_lookup_table.orientation());
  ASSERT_EQ(2UL, target_lookup_table.candidates().size());
  EXPECT_EQ(kSampleText1, target_lookup_table.candidates().at(0).value);
  EXPECT_EQ(kSampleText2, target_lookup_table.candidates().at(1).value);
  EXPECT_EQ(kSampleLabel1, target_lookup_table.candidates().at(0).label);
  EXPECT_EQ(kSampleLabel2, target_lookup_table.candidates().at(1).label);
  const std::string expected_binary(
      reinterpret_cast<const char*>(kSampleBuffer), kSampleBufferLength);
  EXPECT_EQ(expected_binary,
            target_lookup_table.serialized_mozc_candidates_data());
}

}  // namespace ibus
}  // namespace chromeos
