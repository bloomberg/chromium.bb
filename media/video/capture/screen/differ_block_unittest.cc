// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "media/video/capture/screen/differ_block.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// Run 900 times to mimic 1280x720.
// TODO(fbarchard): Remove benchmark once performance is non-issue.
static const int kTimesToRun = 900;

static void GenerateData(uint8* data, int size) {
  for (int i = 0; i < size; ++i) {
    data[i] = i;
  }
}

// Memory buffer large enough for 2 blocks aligned to 16 bytes.
static const int kSizeOfBlock = kBlockSize * kBlockSize * kBytesPerPixel;
uint8 block_buffer[kSizeOfBlock * 2 + 16];

void PrepareBuffers(uint8* &block1, uint8* &block2) {
  block1 = reinterpret_cast<uint8*>
      ((reinterpret_cast<uintptr_t>(&block_buffer[0]) + 15) & ~15);
  GenerateData(block1, kSizeOfBlock);
  block2 = block1 + kSizeOfBlock;
  memcpy(block2, block1, kSizeOfBlock);
}

TEST(BlockDifferenceTestSame, BlockDifference) {
  uint8* block1;
  uint8* block2;
  PrepareBuffers(block1, block2);

  // These blocks should match.
  for (int i = 0; i < kTimesToRun; ++i) {
    int result = BlockDifference(block1, block2, kBlockSize * kBytesPerPixel);
    EXPECT_EQ(0, result);
  }
}

TEST(BlockDifferenceTestLast, BlockDifference) {
  uint8* block1;
  uint8* block2;
  PrepareBuffers(block1, block2);
  block2[kSizeOfBlock-2] += 1;

  for (int i = 0; i < kTimesToRun; ++i) {
    int result = BlockDifference(block1, block2, kBlockSize * kBytesPerPixel);
    EXPECT_EQ(1, result);
  }
}

TEST(BlockDifferenceTestMid, BlockDifference) {
  uint8* block1;
  uint8* block2;
  PrepareBuffers(block1, block2);
  block2[kSizeOfBlock/2+1] += 1;

  for (int i = 0; i < kTimesToRun; ++i) {
    int result = BlockDifference(block1, block2, kBlockSize * kBytesPerPixel);
    EXPECT_EQ(1, result);
  }
}

TEST(BlockDifferenceTestFirst, BlockDifference) {
  uint8* block1;
  uint8* block2;
  PrepareBuffers(block1, block2);
  block2[0] += 1;

  for (int i = 0; i < kTimesToRun; ++i) {
    int result = BlockDifference(block1, block2, kBlockSize * kBytesPerPixel);
    EXPECT_EQ(1, result);
  }
}

}  // namespace media
