// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "cc/playback/display_list_raster_source.h"
#include "cc/test/fake_display_list_recording_source.h"
#include "cc/test/skia_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(DisplayListRecordingSourceTest, DiscardablePixelRefsWithTransform) {
  gfx::Size grid_cell_size(128, 128);
  gfx::Rect recorded_viewport(0, 0, 256, 256);

  scoped_ptr<FakeDisplayListRecordingSource> recording_source =
      FakeDisplayListRecordingSource::CreateRecordingSource(recorded_viewport);
  recording_source->SetGridCellSize(grid_cell_size);
  SkBitmap discardable_bitmap[2][2];
  gfx::Transform identity_transform;
  CreateBitmap(gfx::Size(32, 32), "discardable", &discardable_bitmap[0][0]);
  // Translate transform is equivalent to moving using point.
  gfx::Transform translate_transform;
  translate_transform.Translate(0, 130);
  CreateBitmap(gfx::Size(32, 32), "discardable", &discardable_bitmap[1][0]);
  // This moves the bitmap to center of viewport and rotate, this would make
  // this bitmap in all four tile grids.
  gfx::Transform rotate_transform;
  rotate_transform.Translate(112, 112);
  rotate_transform.Rotate(45);
  CreateBitmap(gfx::Size(32, 32), "discardable", &discardable_bitmap[1][1]);

  recording_source->add_draw_bitmap_with_transform(discardable_bitmap[0][0],
                                                   identity_transform);
  recording_source->add_draw_bitmap_with_transform(discardable_bitmap[1][0],
                                                   translate_transform);
  recording_source->add_draw_bitmap_with_transform(discardable_bitmap[1][1],
                                                   rotate_transform);
  recording_source->SetGatherPixelRefs(true);
  recording_source->Rerecord();

  bool can_use_lcd_text = true;
  scoped_refptr<DisplayListRasterSource> raster_source =
      DisplayListRasterSource::CreateFromDisplayListRecordingSource(
          recording_source.get(), can_use_lcd_text);

  // Tile sized iterators. These should find only one pixel ref.
  {
    std::vector<SkPixelRef*> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(0, 0, 128, 128), 1.0, &pixel_refs);
    EXPECT_FALSE(pixel_refs.empty());
    EXPECT_TRUE(pixel_refs[0] == discardable_bitmap[0][0].pixelRef());
    EXPECT_TRUE(pixel_refs[1] == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(2u, pixel_refs.size());
  }
  {
    std::vector<SkPixelRef*> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(0, 0, 256, 256), 2.0, &pixel_refs);
    EXPECT_FALSE(pixel_refs.empty());
    EXPECT_TRUE(pixel_refs[0] == discardable_bitmap[0][0].pixelRef());
    EXPECT_TRUE(pixel_refs[1] == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(2u, pixel_refs.size());
  }
  {
    std::vector<SkPixelRef*> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(0, 0, 64, 64), 0.5, &pixel_refs);
    EXPECT_FALSE(pixel_refs.empty());
    EXPECT_TRUE(pixel_refs[0] == discardable_bitmap[0][0].pixelRef());
    EXPECT_TRUE(pixel_refs[1] == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(2u, pixel_refs.size());
  }

  // Shifted tile sized iterators. These should find only one pixel ref.
  {
    std::vector<SkPixelRef*> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(140, 140, 128, 128), 1.0,
                                   &pixel_refs);
    EXPECT_FALSE(pixel_refs.empty());
    EXPECT_TRUE(pixel_refs[0] == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(1u, pixel_refs.size());
  }
  {
    std::vector<SkPixelRef*> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(280, 280, 256, 256), 2.0,
                                   &pixel_refs);
    EXPECT_FALSE(pixel_refs.empty());
    EXPECT_TRUE(pixel_refs[0] == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(1u, pixel_refs.size());
  }
  {
    std::vector<SkPixelRef*> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(70, 70, 64, 64), 0.5, &pixel_refs);
    EXPECT_FALSE(pixel_refs.empty());
    EXPECT_TRUE(pixel_refs[0] == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(1u, pixel_refs.size());
  }

  // The rotated bitmap would still be in the top right tile.
  {
    std::vector<SkPixelRef*> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(140, 0, 128, 128), 1.0,
                                   &pixel_refs);
    EXPECT_FALSE(pixel_refs.empty());
    EXPECT_TRUE(pixel_refs[0] == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(1u, pixel_refs.size());
  }

  // Layer sized iterators. These should find all 6 pixel refs, including 1
  // pixel ref bitmap[0][0], 1 pixel ref for bitmap[1][0], and 4 pixel refs for
  // bitmap[1][1].
  {
    std::vector<SkPixelRef*> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(0, 0, 256, 256), 1.0, &pixel_refs);
    EXPECT_FALSE(pixel_refs.empty());
    // Top left tile with bitmap[0][0] and bitmap[1][1].
    EXPECT_TRUE(pixel_refs[0] == discardable_bitmap[0][0].pixelRef());
    EXPECT_TRUE(pixel_refs[1] == discardable_bitmap[1][1].pixelRef());
    // Top right tile with bitmap[1][1].
    EXPECT_TRUE(pixel_refs[2] == discardable_bitmap[1][1].pixelRef());
    // Bottom left tile with bitmap[1][0] and bitmap[1][1].
    EXPECT_TRUE(pixel_refs[3] == discardable_bitmap[1][0].pixelRef());
    EXPECT_TRUE(pixel_refs[4] == discardable_bitmap[1][1].pixelRef());
    // Bottom right tile with bitmap[1][1].
    EXPECT_TRUE(pixel_refs[5] == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(6u, pixel_refs.size());
  }
  {
    std::vector<SkPixelRef*> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(0, 0, 512, 512), 2.0, &pixel_refs);
    EXPECT_FALSE(pixel_refs.empty());
    // Top left tile with bitmap[0][0] and bitmap[1][1].
    EXPECT_TRUE(pixel_refs[0] == discardable_bitmap[0][0].pixelRef());
    EXPECT_TRUE(pixel_refs[1] == discardable_bitmap[1][1].pixelRef());
    // Top right tile with bitmap[1][1].
    EXPECT_TRUE(pixel_refs[2] == discardable_bitmap[1][1].pixelRef());
    // Bottom left tile with bitmap[1][0] and bitmap[1][1].
    EXPECT_TRUE(pixel_refs[3] == discardable_bitmap[1][0].pixelRef());
    EXPECT_TRUE(pixel_refs[4] == discardable_bitmap[1][1].pixelRef());
    // Bottom right tile with bitmap[1][1].
    EXPECT_TRUE(pixel_refs[5] == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(6u, pixel_refs.size());
  }
  {
    std::vector<SkPixelRef*> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(0, 0, 128, 128), 0.5, &pixel_refs);
    EXPECT_FALSE(pixel_refs.empty());
    // Top left tile with bitmap[0][0] and bitmap[1][1].
    EXPECT_TRUE(pixel_refs[0] == discardable_bitmap[0][0].pixelRef());
    EXPECT_TRUE(pixel_refs[1] == discardable_bitmap[1][1].pixelRef());
    // Top right tile with bitmap[1][1].
    EXPECT_TRUE(pixel_refs[2] == discardable_bitmap[1][1].pixelRef());
    // Bottom left tile with bitmap[1][0] and bitmap[1][1].
    EXPECT_TRUE(pixel_refs[3] == discardable_bitmap[1][0].pixelRef());
    EXPECT_TRUE(pixel_refs[4] == discardable_bitmap[1][1].pixelRef());
    // Bottom right tile with bitmap[1][1].
    EXPECT_TRUE(pixel_refs[5] == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(6u, pixel_refs.size());
  }
}

}  // namespace
}  // namespace cc
