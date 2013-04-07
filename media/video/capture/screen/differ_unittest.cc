// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "media/video/capture/screen/differ.h"
#include "media/video/capture/screen/differ_block.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// 96x96 screen gives a 4x4 grid of blocks.
const int kScreenWidth= 96;
const int kScreenHeight = 96;

// To test partial blocks, we need a width and height that are not multiples
// of 16 (or 32, depending on current block size).
const int kPartialScreenWidth = 70;
const int kPartialScreenHeight = 70;

class DifferTest : public testing::Test {
 public:
  DifferTest() {
  }

 protected:
  void InitDiffer(int width, int height) {
    width_ = width;
    height_ = height;
    bytes_per_pixel_ = kBytesPerPixel;
    stride_ = (kBytesPerPixel * width);
    buffer_size_ = width_ * height_ * bytes_per_pixel_;

    differ_.reset(new Differ(width_, height_, bytes_per_pixel_, stride_));

    prev_.reset(new uint8[buffer_size_]);
    memset(prev_.get(), 0, buffer_size_);

    curr_.reset(new uint8[buffer_size_]);
    memset(curr_.get(), 0, buffer_size_);
  }

  void ClearBuffer(uint8* buffer) {
    memset(buffer, 0, buffer_size_);
  }

  // Here in DifferTest so that tests can access private methods of Differ.
  void MarkDirtyBlocks(const void* prev_buffer, const void* curr_buffer) {
    differ_->MarkDirtyBlocks(prev_buffer, curr_buffer);
  }

  void MergeBlocks(SkRegion* dirty) {
    differ_->MergeBlocks(dirty);
  }

  // Convenience method to count rectangles in a region.
  int RegionRectCount(const SkRegion& region) {
    int count = 0;
    for(SkRegion::Iterator iter(region); !iter.done(); iter.next()) {
      ++count;
    }
    return count;
  }

  // Convenience wrapper for Differ's DiffBlock that calculates the appropriate
  // offset to the start of the desired block.
  DiffInfo DiffBlock(int block_x, int block_y) {
    // Offset from upper-left of buffer to upper-left of requested block.
    int block_offset = ((block_y * stride_) + (block_x * bytes_per_pixel_))
                        * kBlockSize;
    return BlockDifference(prev_.get() + block_offset,
                           curr_.get() + block_offset,
                           stride_);
  }

  // Write the pixel |value| into the specified block in the |buffer|.
  // This is a convenience wrapper around WritePixel().
  void WriteBlockPixel(uint8* buffer, int block_x, int block_y,
                       int pixel_x, int pixel_y, uint32 value) {
    WritePixel(buffer, (block_x * kBlockSize) + pixel_x,
               (block_y * kBlockSize) + pixel_y, value);
  }

  // Write the test pixel |value| into the |buffer| at the specified |x|,|y|
  // location.
  // Only the low-order bytes from |value| are written (assuming little-endian).
  // So, for |value| = 0xaabbccdd:
  //   If bytes_per_pixel = 4, then ddccbbaa will be written as the pixel value.
  //   If                 = 3,        ddccbb
  //   If                 = 2,          ddcc
  //   If                 = 1,            dd
  void WritePixel(uint8* buffer, int x, int y, uint32 value) {
    uint8* pixel = reinterpret_cast<uint8*>(&value);
    buffer += (y * stride_) + (x * bytes_per_pixel_);
    for (int b = bytes_per_pixel_ - 1; b >= 0; b--) {
      *buffer++ = pixel[b];
    }
  }

  // DiffInfo utility routines.
  // These are here so that we don't have to make each DifferText_Xxx_Test
  // class a friend class to Differ.

  // Clear out the entire |diff_info_| buffer.
  void ClearDiffInfo() {
    memset(differ_->diff_info_.get(), 0, differ_->diff_info_size_);
  }

  // Get the value in the |diff_info_| array at (x,y).
  DiffInfo GetDiffInfo(int x, int y) {
    DiffInfo* diff_info = differ_->diff_info_.get();
    return diff_info[(y * GetDiffInfoWidth()) + x];
  }

  // Width of |diff_info_| array.
  int GetDiffInfoWidth() {
    return differ_->diff_info_width_;
  }

  // Height of |diff_info_| array.
  int GetDiffInfoHeight() {
    return differ_->diff_info_height_;
  }

  // Size of |diff_info_| array.
  int GetDiffInfoSize() {
    return differ_->diff_info_size_;
  }

  void SetDiffInfo(int x, int y, const DiffInfo& value) {
    DiffInfo* diff_info = differ_->diff_info_.get();
    diff_info[(y * GetDiffInfoWidth()) + x] = value;
  }

  // Mark the range of blocks specified.
  void MarkBlocks(int x_origin, int y_origin, int width, int height) {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        SetDiffInfo(x_origin + x, y_origin + y, 1);
      }
    }
  }

  // Verify that |region| contains a rectangle defined by |x|, |y|, |width| and
  // |height|.
  // |x|, |y|, |width| and |height| are specified in block (not pixel) units.
  bool CheckDirtyRegionContainsRect(const SkRegion& region, int x, int y,
                                    int width, int height) {
    SkIRect r = SkIRect::MakeXYWH(x * kBlockSize, y * kBlockSize,
                                  width * kBlockSize, height * kBlockSize);
    bool found = false;
    for (SkRegion::Iterator i(region); !found && !i.done(); i.next()) {
      found = (i.rect() == r);
    }
    return found;
  }

  // Mark the range of blocks specified and then verify that they are
  // merged correctly.
  // Only one rectangular region of blocks can be checked with this routine.
  bool MarkBlocksAndCheckMerge(int x_origin, int y_origin,
                               int width, int height) {
    ClearDiffInfo();
    MarkBlocks(x_origin, y_origin, width, height);

    SkRegion dirty;
    MergeBlocks(&dirty);

    bool is_good = dirty.isRect();
    if (is_good) {
     is_good = CheckDirtyRegionContainsRect(dirty, x_origin, y_origin,
                                            width, height);
    }
    return is_good;
  }

  // The differ class we're testing.
  scoped_ptr<Differ> differ_;

  // Screen/buffer info.
  int width_;
  int height_;
  int bytes_per_pixel_;
  int stride_;

  // Size of each screen buffer.
  int buffer_size_;

  // Previous and current screen buffers.
  scoped_ptr<uint8[]> prev_;
  scoped_ptr<uint8[]> curr_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DifferTest);
};

TEST_F(DifferTest, Setup) {
  InitDiffer(kScreenWidth, kScreenHeight);
  // 96x96 pixels results in 3x3 array. Add 1 to each dimension as boundary.
  // +---+---+---+---+
  // | o | o | o | _ |
  // +---+---+---+---+  o = blocks mapped to screen pixels
  // | o | o | o | _ |
  // +---+---+---+---+  _ = boundary blocks
  // | o | o | o | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  EXPECT_EQ(4, GetDiffInfoWidth());
  EXPECT_EQ(4, GetDiffInfoHeight());
  EXPECT_EQ(16, GetDiffInfoSize());
}

TEST_F(DifferTest, MarkDirtyBlocks_All) {
  InitDiffer(kScreenWidth, kScreenHeight);
  ClearDiffInfo();

  // Update a pixel in each block.
  for (int y = 0; y < GetDiffInfoHeight() - 1; y++) {
    for (int x = 0; x < GetDiffInfoWidth() - 1; x++) {
      WriteBlockPixel(curr_.get(), x, y, 10, 10, 0xff00ff);
    }
  }

  MarkDirtyBlocks(prev_.get(), curr_.get());

  // Make sure each block is marked as dirty.
  for (int y = 0; y < GetDiffInfoHeight() - 1; y++) {
    for (int x = 0; x < GetDiffInfoWidth() - 1; x++) {
      EXPECT_EQ(1, GetDiffInfo(x, y))
          << "when x = " << x << ", and y = " << y;
    }
  }
}

TEST_F(DifferTest, MarkDirtyBlocks_Sampling) {
  InitDiffer(kScreenWidth, kScreenHeight);
  ClearDiffInfo();

  // Update some pixels in image.
  WriteBlockPixel(curr_.get(), 1, 0, 10, 10, 0xff00ff);
  WriteBlockPixel(curr_.get(), 2, 1, 10, 10, 0xff00ff);
  WriteBlockPixel(curr_.get(), 0, 2, 10, 10, 0xff00ff);

  MarkDirtyBlocks(prev_.get(), curr_.get());

  // Make sure corresponding blocks are updated.
  EXPECT_EQ(0, GetDiffInfo(0, 0));
  EXPECT_EQ(0, GetDiffInfo(0, 1));
  EXPECT_EQ(1, GetDiffInfo(0, 2));
  EXPECT_EQ(1, GetDiffInfo(1, 0));
  EXPECT_EQ(0, GetDiffInfo(1, 1));
  EXPECT_EQ(0, GetDiffInfo(1, 2));
  EXPECT_EQ(0, GetDiffInfo(2, 0));
  EXPECT_EQ(1, GetDiffInfo(2, 1));
  EXPECT_EQ(0, GetDiffInfo(2, 2));
}

TEST_F(DifferTest, DiffBlock) {
  InitDiffer(kScreenWidth, kScreenHeight);

  // Verify no differences at start.
  EXPECT_EQ(0, DiffBlock(0, 0));
  EXPECT_EQ(0, DiffBlock(1, 1));

  // Write new data into the 4 corners of the middle block and verify that
  // neighboring blocks are not affected.
  int max = kBlockSize - 1;
  WriteBlockPixel(curr_.get(), 1, 1, 0, 0, 0xffffff);
  WriteBlockPixel(curr_.get(), 1, 1, 0, max, 0xffffff);
  WriteBlockPixel(curr_.get(), 1, 1, max, 0, 0xffffff);
  WriteBlockPixel(curr_.get(), 1, 1, max, max, 0xffffff);
  EXPECT_EQ(0, DiffBlock(0, 0));
  EXPECT_EQ(0, DiffBlock(0, 1));
  EXPECT_EQ(0, DiffBlock(0, 2));
  EXPECT_EQ(0, DiffBlock(1, 0));
  EXPECT_EQ(1, DiffBlock(1, 1));  // Only this block should change.
  EXPECT_EQ(0, DiffBlock(1, 2));
  EXPECT_EQ(0, DiffBlock(2, 0));
  EXPECT_EQ(0, DiffBlock(2, 1));
  EXPECT_EQ(0, DiffBlock(2, 2));
}

TEST_F(DifferTest, Partial_Setup) {
  InitDiffer(kPartialScreenWidth, kPartialScreenHeight);
  // 70x70 pixels results in 3x3 array: 2x2 full blocks + partials around
  // the edge. One more is added to each dimension as a boundary.
  // +---+---+---+---+
  // | o | o | + | _ |
  // +---+---+---+---+  o = blocks mapped to screen pixels
  // | o | o | + | _ |
  // +---+---+---+---+  + = partial blocks (top/left mapped to screen pixels)
  // | + | + | + | _ |
  // +---+---+---+---+  _ = boundary blocks
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  EXPECT_EQ(4, GetDiffInfoWidth());
  EXPECT_EQ(4, GetDiffInfoHeight());
  EXPECT_EQ(16, GetDiffInfoSize());
}

TEST_F(DifferTest, Partial_FirstPixel) {
  InitDiffer(kPartialScreenWidth, kPartialScreenHeight);
  ClearDiffInfo();

  // Update the first pixel in each block.
  for (int y = 0; y < GetDiffInfoHeight() - 1; y++) {
    for (int x = 0; x < GetDiffInfoWidth() - 1; x++) {
      WriteBlockPixel(curr_.get(), x, y, 0, 0, 0xff00ff);
    }
  }

  MarkDirtyBlocks(prev_.get(), curr_.get());

  // Make sure each block is marked as dirty.
  for (int y = 0; y < GetDiffInfoHeight() - 1; y++) {
    for (int x = 0; x < GetDiffInfoWidth() - 1; x++) {
      EXPECT_EQ(1, GetDiffInfo(x, y))
          << "when x = " << x << ", and y = " << y;
    }
  }
}

TEST_F(DifferTest, Partial_BorderPixel) {
  InitDiffer(kPartialScreenWidth, kPartialScreenHeight);
  ClearDiffInfo();

  // Update the right/bottom border pixels.
  for (int y = 0; y < height_; y++) {
    WritePixel(curr_.get(), width_ - 1, y, 0xff00ff);
  }
  for (int x = 0; x < width_; x++) {
    WritePixel(curr_.get(), x, height_ - 1, 0xff00ff);
  }

  MarkDirtyBlocks(prev_.get(), curr_.get());

  // Make sure last (partial) block in each row/column is marked as dirty.
  int x_last = GetDiffInfoWidth() - 2;
  for (int y = 0; y < GetDiffInfoHeight() - 1; y++) {
    EXPECT_EQ(1, GetDiffInfo(x_last, y))
        << "when x = " << x_last << ", and y = " << y;
  }
  int y_last = GetDiffInfoHeight() - 2;
  for (int x = 0; x < GetDiffInfoWidth() - 1; x++) {
    EXPECT_EQ(1, GetDiffInfo(x, y_last))
        << "when x = " << x << ", and y = " << y_last;
  }
  // All other blocks are clean.
  for (int y = 0; y < GetDiffInfoHeight() - 2; y++) {
    for (int x = 0; x < GetDiffInfoWidth() - 2; x++) {
      EXPECT_EQ(0, GetDiffInfo(x, y)) << "when x = " << x << ", and y = " << y;
    }
  }
}

TEST_F(DifferTest, MergeBlocks_Empty) {
  InitDiffer(kScreenWidth, kScreenHeight);

  // No blocks marked:
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ClearDiffInfo();

  SkRegion dirty;
  MergeBlocks(&dirty);

  EXPECT_TRUE(dirty.isEmpty());
}

TEST_F(DifferTest, MergeBlocks_SingleBlock) {
  InitDiffer(kScreenWidth, kScreenHeight);
  // Mark a single block and make sure that there is a single merged
  // rect with the correct bounds.
  for (int y = 0; y < GetDiffInfoHeight() - 1; y++) {
    for (int x = 0; x < GetDiffInfoWidth() - 1; x++) {
      ASSERT_TRUE(MarkBlocksAndCheckMerge(x, y, 1, 1)) << "x: " << x
                                                       << "y: " << y;
    }
  }
}

TEST_F(DifferTest, MergeBlocks_BlockRow) {
  InitDiffer(kScreenWidth, kScreenHeight);

  // +---+---+---+---+
  // | X | X |   | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ASSERT_TRUE(MarkBlocksAndCheckMerge(0, 0, 2, 1));

  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // | X | X | X | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ASSERT_TRUE(MarkBlocksAndCheckMerge(0, 1, 3, 1));

  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // |   | X | X | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ASSERT_TRUE(MarkBlocksAndCheckMerge(1, 2, 2, 1));
}

TEST_F(DifferTest, MergeBlocks_BlockColumn) {
  InitDiffer(kScreenWidth, kScreenHeight);

  // +---+---+---+---+
  // | X |   |   | _ |
  // +---+---+---+---+
  // | X |   |   | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ASSERT_TRUE(MarkBlocksAndCheckMerge(0, 0, 1, 2));

  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // |   | X |   | _ |
  // +---+---+---+---+
  // |   | X |   | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ASSERT_TRUE(MarkBlocksAndCheckMerge(1, 1, 1, 2));

  // +---+---+---+---+
  // |   |   | X | _ |
  // +---+---+---+---+
  // |   |   | X | _ |
  // +---+---+---+---+
  // |   |   | X | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ASSERT_TRUE(MarkBlocksAndCheckMerge(2, 0, 1, 3));
}

TEST_F(DifferTest, MergeBlocks_BlockRect) {
  InitDiffer(kScreenWidth, kScreenHeight);

  // +---+---+---+---+
  // | X | X |   | _ |
  // +---+---+---+---+
  // | X | X |   | _ |
  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ASSERT_TRUE(MarkBlocksAndCheckMerge(0, 0, 2, 2));

  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // |   | X | X | _ |
  // +---+---+---+---+
  // |   | X | X | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ASSERT_TRUE(MarkBlocksAndCheckMerge(1, 1, 2, 2));

  // +---+---+---+---+
  // |   | X | X | _ |
  // +---+---+---+---+
  // |   | X | X | _ |
  // +---+---+---+---+
  // |   | X | X | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ASSERT_TRUE(MarkBlocksAndCheckMerge(1, 0, 2, 3));

  // +---+---+---+---+
  // |   |   |   | _ |
  // +---+---+---+---+
  // | X | X | X | _ |
  // +---+---+---+---+
  // | X | X | X | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ASSERT_TRUE(MarkBlocksAndCheckMerge(0, 1, 3, 2));

  // +---+---+---+---+
  // | X | X | X | _ |
  // +---+---+---+---+
  // | X | X | X | _ |
  // +---+---+---+---+
  // | X | X | X | _ |
  // +---+---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ASSERT_TRUE(MarkBlocksAndCheckMerge(0, 0, 3, 3));
}

// This tests marked regions that require more than 1 single dirty rect.
// The exact rects returned depend on the current implementation, so these
// may need to be updated if we modify how we merge blocks.
TEST_F(DifferTest, MergeBlocks_MultiRect) {
  InitDiffer(kScreenWidth, kScreenHeight);
  SkRegion dirty;

  // +---+---+---+---+      +---+---+---+
  // |   | X |   | _ |      |   | 0 |   |
  // +---+---+---+---+      +---+---+---+
  // | X |   |   | _ |      | 1 |   |   |
  // +---+---+---+---+  =>  +---+---+---+
  // |   |   | X | _ |      |   |   | 2 |
  // +---+---+---+---+      +---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ClearDiffInfo();
  MarkBlocks(1, 0, 1, 1);
  MarkBlocks(0, 1, 1, 1);
  MarkBlocks(2, 2, 1, 1);

  dirty.setEmpty();
  MergeBlocks(&dirty);

  ASSERT_EQ(3, RegionRectCount(dirty));
  ASSERT_TRUE(CheckDirtyRegionContainsRect(dirty, 1, 0, 1, 1));
  ASSERT_TRUE(CheckDirtyRegionContainsRect(dirty, 0, 1, 1, 1));
  ASSERT_TRUE(CheckDirtyRegionContainsRect(dirty, 2, 2, 1, 1));

  // +---+---+---+---+      +---+---+---+
  // |   |   | X | _ |      |   |   | 0 |
  // +---+---+---+---+      +---+---+---+
  // | X | X | X | _ |      | 1   1   1 |
  // +---+---+---+---+  =>  +           +
  // | X | X | X | _ |      | 1   1   1 |
  // +---+---+---+---+      +---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ClearDiffInfo();
  MarkBlocks(2, 0, 1, 1);
  MarkBlocks(0, 1, 3, 2);

  dirty.setEmpty();
  MergeBlocks(&dirty);

  ASSERT_EQ(2, RegionRectCount(dirty));
  ASSERT_TRUE(CheckDirtyRegionContainsRect(dirty, 2, 0, 1, 1));
  ASSERT_TRUE(CheckDirtyRegionContainsRect(dirty, 0, 1, 3, 2));

  // +---+---+---+---+      +---+---+---+
  // |   |   |   | _ |      |   |   |   |
  // +---+---+---+---+      +---+---+---+
  // | X |   | X | _ |      | 0 |   | 1 |
  // +---+---+---+---+  =>  +   +---+   +
  // | X | X | X | _ |      | 2 | 2 | 2 |
  // +---+---+---+---+      +---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ClearDiffInfo();
  MarkBlocks(0, 1, 1, 1);
  MarkBlocks(2, 1, 1, 1);
  MarkBlocks(0, 2, 3, 1);

  dirty.setEmpty();
  MergeBlocks(&dirty);

  ASSERT_EQ(3, RegionRectCount(dirty));
  ASSERT_TRUE(CheckDirtyRegionContainsRect(dirty, 0, 1, 1, 1));
  ASSERT_TRUE(CheckDirtyRegionContainsRect(dirty, 2, 1, 1, 1));
  ASSERT_TRUE(CheckDirtyRegionContainsRect(dirty, 0, 2, 3, 1));

  // +---+---+---+---+      +---+---+---+
  // | X | X | X | _ |      | 0   0   0 |
  // +---+---+---+---+      +---+---+---+
  // | X |   | X | _ |      | 1 |   | 2 |
  // +---+---+---+---+  =>  +   +---+   +
  // | X | X | X | _ |      | 3 | 3 | 3 |
  // +---+---+---+---+      +---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ClearDiffInfo();
  MarkBlocks(0, 0, 3, 1);
  MarkBlocks(0, 1, 1, 1);
  MarkBlocks(2, 1, 1, 1);
  MarkBlocks(0, 2, 3, 1);

  dirty.setEmpty();
  MergeBlocks(&dirty);

  ASSERT_EQ(4, RegionRectCount(dirty));
  ASSERT_TRUE(CheckDirtyRegionContainsRect(dirty, 0, 0, 3, 1));
  ASSERT_TRUE(CheckDirtyRegionContainsRect(dirty, 0, 1, 1, 1));
  ASSERT_TRUE(CheckDirtyRegionContainsRect(dirty, 2, 1, 1, 1));
  ASSERT_TRUE(CheckDirtyRegionContainsRect(dirty, 0, 2, 3, 1));

  // +---+---+---+---+      +---+---+---+
  // | X | X |   | _ |      | 0   0 |   |
  // +---+---+---+---+      +       +---+
  // | X | X |   | _ |      | 0   0 |   |
  // +---+---+---+---+  =>  +---+---+---+
  // |   | X |   | _ |      |   | 1 |   |
  // +---+---+---+---+      +---+---+---+
  // | _ | _ | _ | _ |
  // +---+---+---+---+
  ClearDiffInfo();
  MarkBlocks(0, 0, 2, 2);
  MarkBlocks(1, 2, 1, 1);

  dirty.setEmpty();
  MergeBlocks(&dirty);

  ASSERT_EQ(2, RegionRectCount(dirty));
  ASSERT_TRUE(CheckDirtyRegionContainsRect(dirty, 0, 0, 2, 2));
  ASSERT_TRUE(CheckDirtyRegionContainsRect(dirty, 1, 2, 1, 1));
}

}  // namespace media
