// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/write_blocked_list.h"

#include <deque>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

class WriteBlockedListPeer {
 public:
  static std::deque<int>* GetWriteBlockedList(
      int i,
      net::WriteBlockedList<int>* list) {
    return &list->write_blocked_lists_[i];
  }
};

}  // namespace

namespace net {
namespace test {
namespace {
typedef WriteBlockedList<int> IntWriteBlockedList;

TEST(WriteBlockedListTest, GetHighestPriority) {
  IntWriteBlockedList list;
  EXPECT_FALSE(list.HasWriteBlockedStreams());
  list.PushBack(1, 1);
  EXPECT_TRUE(list.HasWriteBlockedStreams());
  EXPECT_EQ(1, list.GetHighestPriorityWriteBlockedList());
  list.PushBack(1, 0);
  EXPECT_TRUE(list.HasWriteBlockedStreams());
  EXPECT_EQ(0, list.GetHighestPriorityWriteBlockedList());
}

TEST(WriteBlockedListTest, HasWriteBlockedStreamsOfGreaterThanPriority) {
  IntWriteBlockedList list;
  list.PushBack(1, 4);
  EXPECT_TRUE(list.HasWriteBlockedStreamsGreaterThanPriority(5));
  EXPECT_FALSE(list.HasWriteBlockedStreamsGreaterThanPriority(4));
  list.PushBack(1, 2);
  EXPECT_TRUE(list.HasWriteBlockedStreamsGreaterThanPriority(3));
  EXPECT_FALSE(list.HasWriteBlockedStreamsGreaterThanPriority(2));
}

TEST(WriteBlockedListTest, RemoveStreamFromWriteBlockedList) {
  IntWriteBlockedList list;

  list.PushBack(1, 4);
  EXPECT_TRUE(list.HasWriteBlockedStreams());

  list.RemoveStreamFromWriteBlockedList(1, 5);
  EXPECT_TRUE(list.HasWriteBlockedStreams());

  list.PushBack(2, 4);
  list.PushBack(1, 4);
  list.RemoveStreamFromWriteBlockedList(1, 4);
  list.RemoveStreamFromWriteBlockedList(2, 4);
  EXPECT_FALSE(list.HasWriteBlockedStreams());

  list.PushBack(1, 7);
  EXPECT_TRUE(list.HasWriteBlockedStreams());
}

TEST(WriteBlockedListTest, PopFront) {
  IntWriteBlockedList list;

  list.PushBack(1, 4);
  EXPECT_EQ(1u, list.NumBlockedStreams());
  list.PushBack(2, 4);
  list.PushBack(1, 4);
  list.PushBack(3, 4);
  EXPECT_EQ(4u, list.NumBlockedStreams());

  EXPECT_EQ(1, list.PopFront(4));
  EXPECT_EQ(2, list.PopFront(4));
  EXPECT_EQ(1, list.PopFront(4));
  EXPECT_EQ(1u, list.NumBlockedStreams());
  EXPECT_EQ(3, list.PopFront(4));
}

TEST(WriteBlockedListTest, UpdateStreamPriorityInWriteBlockedList) {
  IntWriteBlockedList list;

  list.PushBack(1, 1);
  list.PushBack(2, 2);
  list.PushBack(3, 3);
  list.PushBack(1, 3);
  list.PushBack(1, 3);
  EXPECT_EQ(5u, list.NumBlockedStreams());
  EXPECT_EQ(1u, WriteBlockedListPeer::GetWriteBlockedList(1, &list)->size());
  EXPECT_EQ(1u, WriteBlockedListPeer::GetWriteBlockedList(2, &list)->size());
  EXPECT_EQ(3u, WriteBlockedListPeer::GetWriteBlockedList(3, &list)->size());

  list.UpdateStreamPriorityInWriteBlockedList(1, 1, 2);
  EXPECT_EQ(0u, WriteBlockedListPeer::GetWriteBlockedList(1, &list)->size());
  EXPECT_EQ(2u, WriteBlockedListPeer::GetWriteBlockedList(2, &list)->size());
  list.UpdateStreamPriorityInWriteBlockedList(3, 3, 1);
  EXPECT_EQ(1u, WriteBlockedListPeer::GetWriteBlockedList(1, &list)->size());
  EXPECT_EQ(2u, WriteBlockedListPeer::GetWriteBlockedList(3, &list)->size());

  // Redundant update.
  list.UpdateStreamPriorityInWriteBlockedList(1, 3, 3);
  EXPECT_EQ(2u, WriteBlockedListPeer::GetWriteBlockedList(3, &list)->size());

  // No entries for given stream_id / old_priority pair.
  list.UpdateStreamPriorityInWriteBlockedList(4, 4, 1);
  EXPECT_EQ(1u, WriteBlockedListPeer::GetWriteBlockedList(1, &list)->size());
  EXPECT_EQ(2u, WriteBlockedListPeer::GetWriteBlockedList(2, &list)->size());
  EXPECT_EQ(0u, WriteBlockedListPeer::GetWriteBlockedList(4, &list)->size());

  // Update multiple entries.
  list.UpdateStreamPriorityInWriteBlockedList(1, 3, 4);
  EXPECT_EQ(0u, WriteBlockedListPeer::GetWriteBlockedList(3, &list)->size());
  EXPECT_EQ(1u, WriteBlockedListPeer::GetWriteBlockedList(4, &list)->size());

  EXPECT_EQ(3, list.PopFront(1));
  EXPECT_EQ(2, list.PopFront(2));
  EXPECT_EQ(1, list.PopFront(2));
  EXPECT_EQ(1, list.PopFront(4));
  EXPECT_EQ(0u, list.NumBlockedStreams());
}

}  // namespace
}  // namespace test
}  // namespace net
