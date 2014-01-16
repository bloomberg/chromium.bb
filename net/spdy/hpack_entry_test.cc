// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_entry.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using std::string;

const char kName[] = "headername";
const uint32 kNameStringLength = arraysize(kName) - 1;
const char kValue[] = "Header Value";
const uint32 kValueStringLength = arraysize(kValue) - 1;

// Make sure a default-constructed entry is still valid and starts off
// empty, unreferenced, and untouched.
TEST(HpackEntryTest, DefaultConstructor) {
  HpackEntry entry;
  EXPECT_TRUE(entry.name().empty());
  EXPECT_TRUE(entry.value().empty());
  EXPECT_FALSE(entry.IsReferenced());
  EXPECT_EQ(HpackEntry::kUntouched, entry.TouchCount());
  EXPECT_EQ(HpackEntry::kSizeOverhead, entry.Size());
}

// Make sure a non-default-constructed HpackEntry starts off with
// copies of the given name and value, and unreferenced and untouched.
TEST(HpackEntryTest, NormalConstructor) {
  string name = kName;
  string value = kValue;
  HpackEntry entry(name, value);
  EXPECT_EQ(name, entry.name());
  EXPECT_EQ(value, entry.value());

  ++name[0];
  ++value[0];
  EXPECT_NE(name, entry.name());
  EXPECT_NE(value, entry.name());

  EXPECT_FALSE(entry.IsReferenced());
  EXPECT_EQ(HpackEntry::kUntouched, entry.TouchCount());
  EXPECT_EQ(
      kNameStringLength + kValueStringLength + HpackEntry::kSizeOverhead,
      entry.Size());
}

// Make sure twiddling the referenced bit doesn't affect the touch
// count when it's kUntouched.
TEST(HpackEntryTest, IsReferencedUntouched) {
  HpackEntry entry(kName, kValue);
  EXPECT_FALSE(entry.IsReferenced());
  EXPECT_EQ(HpackEntry::kUntouched, entry.TouchCount());

  entry.SetReferenced(true);
  EXPECT_TRUE(entry.IsReferenced());
  EXPECT_EQ(HpackEntry::kUntouched, entry.TouchCount());

  entry.SetReferenced(false);
  EXPECT_FALSE(entry.IsReferenced());
  EXPECT_EQ(HpackEntry::kUntouched, entry.TouchCount());
}

// Make sure changing the touch count doesn't affect the referenced
// bit when it's false.
TEST(HpackEntryTest, TouchCountNotReferenced) {
  HpackEntry entry(kName, kValue);
  EXPECT_FALSE(entry.IsReferenced());
  EXPECT_EQ(HpackEntry::kUntouched, entry.TouchCount());

  entry.AddTouches(0);
  EXPECT_FALSE(entry.IsReferenced());
  EXPECT_EQ(0u, entry.TouchCount());

  entry.AddTouches(255);
  EXPECT_FALSE(entry.IsReferenced());
  EXPECT_EQ(255u, entry.TouchCount());

  // Assumes kUntouched is 1 + max touch count.
  entry.AddTouches(HpackEntry::kUntouched - 256);
  EXPECT_FALSE(entry.IsReferenced());
  EXPECT_EQ(HpackEntry::kUntouched - 1, entry.TouchCount());

  entry.ClearTouches();
  EXPECT_FALSE(entry.IsReferenced());
  EXPECT_EQ(HpackEntry::kUntouched, entry.TouchCount());
}

// Make sure changing the touch count doesn't affect the referenced
// bit when it's true.
TEST(HpackEntryTest, TouchCountReferenced) {
  HpackEntry entry(kName, kValue);
  entry.SetReferenced(true);
  EXPECT_TRUE(entry.IsReferenced());
  EXPECT_EQ(HpackEntry::kUntouched, entry.TouchCount());

  entry.AddTouches(0);
  EXPECT_TRUE(entry.IsReferenced());
  EXPECT_EQ(0u, entry.TouchCount());

  entry.AddTouches(255);
  EXPECT_TRUE(entry.IsReferenced());
  EXPECT_EQ(255u, entry.TouchCount());

  // Assumes kUntouched is 1 + max touch count.
  entry.AddTouches(HpackEntry::kUntouched - 256);
  EXPECT_TRUE(entry.IsReferenced());
  EXPECT_EQ(HpackEntry::kUntouched - 1, entry.TouchCount());

  entry.ClearTouches();
  EXPECT_TRUE(entry.IsReferenced());
  EXPECT_EQ(HpackEntry::kUntouched, entry.TouchCount());
}

// Make sure equality takes into account all entry fields.
TEST(HpackEntryTest, Equals) {
  HpackEntry entry1(kName, kValue);
  HpackEntry entry2(kName, kValue);
  EXPECT_TRUE(entry1.Equals(entry2));

  entry2.SetReferenced(true);
  EXPECT_FALSE(entry1.Equals(entry2));
  entry2.SetReferenced(false);
  EXPECT_TRUE(entry1.Equals(entry2));

  entry2.AddTouches(0);
  EXPECT_FALSE(entry1.Equals(entry2));
  entry2.ClearTouches();
  EXPECT_TRUE(entry1.Equals(entry2));

  entry2.AddTouches(1);
  EXPECT_FALSE(entry1.Equals(entry2));
  entry2.ClearTouches();
  EXPECT_TRUE(entry1.Equals(entry2));

  HpackEntry entry3(kName, string(kValue) + kValue);
  EXPECT_FALSE(entry1.Equals(entry3));

  HpackEntry entry4(string(kName) + kName, kValue);
  EXPECT_FALSE(entry1.Equals(entry4));
}

}  // namespace

}  // namespace net
