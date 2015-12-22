// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "chromecast/media/cma/ipc/media_memory_chunk.h"
#include "chromecast/media/cma/ipc/media_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {

class ExternalMemoryBlock
    : public MediaMemoryChunk {
 public:
  ExternalMemoryBlock(void* data, size_t size)
      : data_(data), size_(size) {}
  ~ExternalMemoryBlock() override {}

  // MediaMemoryChunk implementation.
  void* data() const override { return data_; }
  size_t size() const override { return size_; }
  bool valid() const override { return true; }

 private:
  void* const data_;
  const size_t size_;
};

scoped_ptr<MediaMemoryChunk> DummyAllocator(
    void* data, size_t size, size_t alloc_size) {
  CHECK_LE(alloc_size, size);
  return scoped_ptr<MediaMemoryChunk>(
      new ExternalMemoryBlock(data, alloc_size));
}

}

TEST(MediaMessageTest, WriteRead) {
  int buffer_size = 1024;
  scoped_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  MediaMessage::MemoryAllocatorCB mem_alloc_cb(
      base::Bind(&DummyAllocator, buffer.get(), buffer_size));
  uint32_t type = 0x1;
  int msg_content_capacity = 512;

  // Write a message.
  int count = 64;
  scoped_ptr<MediaMessage> msg1(
      MediaMessage::CreateMessage(type, mem_alloc_cb, msg_content_capacity));
  for (int k = 0; k < count; k++) {
    int v1 = 2 * k + 1;
    EXPECT_TRUE(msg1->WritePod(v1));
    uint8_t v2 = k;
    EXPECT_TRUE(msg1->WritePod(v2));
  }
  EXPECT_EQ(msg1->content_size(), count * (sizeof(int) + sizeof(uint8_t)));

  // Verify the integrity of the message.
  scoped_ptr<MediaMessage> msg2(
      MediaMessage::MapMessage(scoped_ptr<MediaMemoryChunk>(
          new ExternalMemoryBlock(&buffer[0], buffer_size))));
  for (int k = 0; k < count; k++) {
    int v1;
    int expected_v1 = 2 * k + 1;
    EXPECT_TRUE(msg2->ReadPod(&v1));
    EXPECT_EQ(v1, expected_v1);
    uint8_t v2;
    uint8_t expected_v2 = k;
    EXPECT_TRUE(msg2->ReadPod(&v2));
    EXPECT_EQ(v2, expected_v2);
  }
}

TEST(MediaMessageTest, WriteOverflow) {
  int buffer_size = 1024;
  scoped_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  MediaMessage::MemoryAllocatorCB mem_alloc_cb(
      base::Bind(&DummyAllocator, buffer.get(), buffer_size));
  uint32_t type = 0x1;
  int msg_content_capacity = 8;

  scoped_ptr<MediaMessage> msg1(
      MediaMessage::CreateMessage(type, mem_alloc_cb, msg_content_capacity));
  uint32_t v1 = 0;
  uint8_t v2 = 0;
  EXPECT_TRUE(msg1->WritePod(v1));
  EXPECT_TRUE(msg1->WritePod(v1));

  EXPECT_FALSE(msg1->WritePod(v1));
  EXPECT_FALSE(msg1->WritePod(v2));
}

TEST(MediaMessageTest, ReadOverflow) {
  int buffer_size = 1024;
  scoped_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  MediaMessage::MemoryAllocatorCB mem_alloc_cb(
      base::Bind(&DummyAllocator, buffer.get(), buffer_size));
  uint32_t type = 0x1;
  int msg_content_capacity = 8;

  scoped_ptr<MediaMessage> msg1(
      MediaMessage::CreateMessage(type, mem_alloc_cb, msg_content_capacity));
  uint32_t v1 = 0xcd;
  EXPECT_TRUE(msg1->WritePod(v1));
  EXPECT_TRUE(msg1->WritePod(v1));

  scoped_ptr<MediaMessage> msg2(
      MediaMessage::MapMessage(scoped_ptr<MediaMemoryChunk>(
          new ExternalMemoryBlock(&buffer[0], buffer_size))));
  uint32_t v2;
  EXPECT_TRUE(msg2->ReadPod(&v2));
  EXPECT_EQ(v2, v1);
  EXPECT_TRUE(msg2->ReadPod(&v2));
  EXPECT_EQ(v2, v1);
  EXPECT_FALSE(msg2->ReadPod(&v2));
}

TEST(MediaMessageTest, DummyMessage) {
  int buffer_size = 1024;
  scoped_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  MediaMessage::MemoryAllocatorCB mem_alloc_cb(
      base::Bind(&DummyAllocator, buffer.get(), buffer_size));
  uint32_t type = 0x1;

  // Create first a dummy message to estimate the content size.
  scoped_ptr<MediaMessage> msg1(
      MediaMessage::CreateDummyMessage(type));
  uint32_t v1 = 0xcd;
  EXPECT_TRUE(msg1->WritePod(v1));
  EXPECT_TRUE(msg1->WritePod(v1));

  // Create the real message and write the actual content.
  scoped_ptr<MediaMessage> msg2(
      MediaMessage::CreateMessage(type, mem_alloc_cb, msg1->content_size()));
  EXPECT_TRUE(msg2->WritePod(v1));
  EXPECT_TRUE(msg2->WritePod(v1));
  EXPECT_FALSE(msg2->WritePod(v1));
}

}  // namespace media
}  // namespace chromecast
