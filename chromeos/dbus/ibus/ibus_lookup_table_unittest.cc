// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(nona): Add more tests.

#include "chromeos/dbus/ibus/ibus_lookup_table.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
TEST(IBusLookupTable, IsEqualTest) {
  IBusLookupTable table1;
  IBusLookupTable table2;

  const char kSampleString1[] = "Sample 1";
  const char kSampleString2[] = "Sample 2";

  EXPECT_TRUE(table1.IsEqual(table2));
  EXPECT_TRUE(table2.IsEqual(table1));

  table1.set_page_size(1);
  table2.set_page_size(2);
  EXPECT_FALSE(table1.IsEqual(table2));
  EXPECT_FALSE(table2.IsEqual(table1));
  table2.set_page_size(1);

  table1.set_cursor_position(1);
  table2.set_cursor_position(2);
  EXPECT_FALSE(table1.IsEqual(table2));
  EXPECT_FALSE(table2.IsEqual(table1));
  table2.set_cursor_position(1);

  table1.set_is_cursor_visible(true);
  table2.set_is_cursor_visible(false);
  EXPECT_FALSE(table1.IsEqual(table2));
  EXPECT_FALSE(table2.IsEqual(table1));
  table2.set_is_cursor_visible(true);

  table1.set_orientation(IBusLookupTable::HORIZONTAL);
  table2.set_orientation(IBusLookupTable::VERTICAL);
  EXPECT_FALSE(table1.IsEqual(table2));
  EXPECT_FALSE(table2.IsEqual(table1));
  table2.set_orientation(IBusLookupTable::HORIZONTAL);

  table1.set_show_window_at_composition(true);
  table2.set_show_window_at_composition(false);
  EXPECT_FALSE(table1.IsEqual(table2));
  EXPECT_FALSE(table2.IsEqual(table1));
  table2.set_show_window_at_composition(true);

  // Check equality for candidates member variable.
  IBusLookupTable::Entry entry1;
  IBusLookupTable::Entry entry2;

  table1.mutable_candidates()->push_back(entry1);
  EXPECT_FALSE(table1.IsEqual(table2));
  EXPECT_FALSE(table2.IsEqual(table1));
  table2.mutable_candidates()->push_back(entry2);
  EXPECT_TRUE(table1.IsEqual(table2));
  EXPECT_TRUE(table2.IsEqual(table1));

  entry1.value = kSampleString1;
  entry2.value = kSampleString2;
  table1.mutable_candidates()->push_back(entry1);
  table2.mutable_candidates()->push_back(entry2);
  EXPECT_FALSE(table1.IsEqual(table2));
  EXPECT_FALSE(table2.IsEqual(table1));
  table1.mutable_candidates()->clear();
  table2.mutable_candidates()->clear();

  entry1.label = kSampleString1;
  entry2.label = kSampleString2;
  table1.mutable_candidates()->push_back(entry1);
  table2.mutable_candidates()->push_back(entry2);
  EXPECT_FALSE(table1.IsEqual(table2));
  EXPECT_FALSE(table2.IsEqual(table1));
  table1.mutable_candidates()->clear();
  table2.mutable_candidates()->clear();

  entry1.annotation = kSampleString1;
  entry2.annotation = kSampleString2;
  table1.mutable_candidates()->push_back(entry1);
  table2.mutable_candidates()->push_back(entry2);
  EXPECT_FALSE(table1.IsEqual(table2));
  EXPECT_FALSE(table2.IsEqual(table1));
  table1.mutable_candidates()->clear();
  table2.mutable_candidates()->clear();

  entry1.description_title = kSampleString1;
  entry2.description_title = kSampleString2;
  table1.mutable_candidates()->push_back(entry1);
  table2.mutable_candidates()->push_back(entry2);
  EXPECT_FALSE(table1.IsEqual(table2));
  EXPECT_FALSE(table2.IsEqual(table1));
  table1.mutable_candidates()->clear();
  table2.mutable_candidates()->clear();

  entry1.description_body = kSampleString1;
  entry2.description_body = kSampleString2;
  table1.mutable_candidates()->push_back(entry1);
  table2.mutable_candidates()->push_back(entry2);
  EXPECT_FALSE(table1.IsEqual(table2));
  EXPECT_FALSE(table2.IsEqual(table1));
  table1.mutable_candidates()->clear();
  table2.mutable_candidates()->clear();
}

TEST(IBusLookupTable, CopyFromTest) {
  IBusLookupTable table1;
  IBusLookupTable table2;

  const char kSampleString[] = "Sample";

  table1.set_page_size(1);
  table1.set_cursor_position(2);
  table1.set_is_cursor_visible(false);
  table1.set_orientation(IBusLookupTable::HORIZONTAL);
  table1.set_show_window_at_composition(false);

  IBusLookupTable::Entry entry;
  entry.value = kSampleString;
  entry.label = kSampleString;
  entry.annotation = kSampleString;
  entry.description_title = kSampleString;
  entry.description_body = kSampleString;
  table1.mutable_candidates()->push_back(entry);

  table2.CopyFrom(table1);
  EXPECT_TRUE(table1.IsEqual(table2));
}
}  // namespace chromeos
