// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "cc/base/region.h"
#include "cc/playback/display_list_raster_source.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_display_list_recording_source.h"
#include "cc/test/skia_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class DisplayListRecordingSourceTest : public testing::Test {
 public:
  void SetUp() override {}
};

TEST_F(DisplayListRecordingSourceTest, DiscardablePixelRefsWithTransform) {
  gfx::Size grid_cell_size(128, 128);
  gfx::Rect recorded_viewport(256, 256);

  scoped_ptr<FakeDisplayListRecordingSource> recording_source =
      FakeDisplayListRecordingSource::CreateFilledRecordingSource(
          recorded_viewport.size());
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

  gfx::RectF rect(0, 0, 32, 32);
  gfx::RectF translate_rect = rect;
  translate_transform.TransformRect(&translate_rect);
  gfx::RectF rotate_rect = rect;
  rotate_transform.TransformRect(&rotate_rect);

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
    std::vector<skia::PositionPixelRef> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(0, 0, 128, 128), 1.0, &pixel_refs);
    EXPECT_EQ(2u, pixel_refs.size());
    EXPECT_TRUE(pixel_refs[0].pixel_ref == discardable_bitmap[0][0].pixelRef());
    EXPECT_TRUE(pixel_refs[1].pixel_ref == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect).ToString());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect).ToString());
  }
  {
    std::vector<skia::PositionPixelRef> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(0, 0, 256, 256), 2.0, &pixel_refs);
    EXPECT_EQ(2u, pixel_refs.size());
    EXPECT_TRUE(pixel_refs[0].pixel_ref == discardable_bitmap[0][0].pixelRef());
    EXPECT_TRUE(pixel_refs[1].pixel_ref == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect).ToString());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect).ToString());
  }
  {
    std::vector<skia::PositionPixelRef> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(0, 0, 64, 64), 0.5, &pixel_refs);
    EXPECT_EQ(2u, pixel_refs.size());
    EXPECT_TRUE(pixel_refs[0].pixel_ref == discardable_bitmap[0][0].pixelRef());
    EXPECT_TRUE(pixel_refs[1].pixel_ref == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect).ToString());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect).ToString());
  }

  // Shifted tile sized iterators. These should find only one pixel ref.
  {
    std::vector<skia::PositionPixelRef> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(140, 140, 128, 128), 1.0,
                                   &pixel_refs);
    EXPECT_EQ(1u, pixel_refs.size());
    EXPECT_TRUE(pixel_refs[0].pixel_ref == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect).ToString());
  }
  {
    std::vector<skia::PositionPixelRef> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(280, 280, 256, 256), 2.0,
                                   &pixel_refs);
    EXPECT_EQ(1u, pixel_refs.size());
    EXPECT_TRUE(pixel_refs[0].pixel_ref == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect).ToString());
  }
  {
    std::vector<skia::PositionPixelRef> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(70, 70, 64, 64), 0.5, &pixel_refs);
    EXPECT_EQ(1u, pixel_refs.size());
    EXPECT_TRUE(pixel_refs[0].pixel_ref == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect).ToString());
  }

  // The rotated bitmap would still be in the top right tile.
  {
    std::vector<skia::PositionPixelRef> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(140, 0, 128, 128), 1.0,
                                   &pixel_refs);
    EXPECT_EQ(1u, pixel_refs.size());
    EXPECT_TRUE(pixel_refs[0].pixel_ref == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect).ToString());
  }

  // Layer sized iterators. These should find all 6 pixel refs, including 1
  // pixel ref bitmap[0][0], 1 pixel ref for bitmap[1][0], and 4 pixel refs for
  // bitmap[1][1].
  {
    std::vector<skia::PositionPixelRef> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(0, 0, 256, 256), 1.0, &pixel_refs);
    EXPECT_EQ(6u, pixel_refs.size());
    // Top left tile with bitmap[0][0] and bitmap[1][1].
    EXPECT_TRUE(pixel_refs[0].pixel_ref == discardable_bitmap[0][0].pixelRef());
    EXPECT_TRUE(pixel_refs[1].pixel_ref == discardable_bitmap[1][1].pixelRef());
    // Top right tile with bitmap[1][1].
    EXPECT_TRUE(pixel_refs[2].pixel_ref == discardable_bitmap[1][1].pixelRef());
    // Bottom left tile with bitmap[1][0] and bitmap[1][1].
    EXPECT_TRUE(pixel_refs[3].pixel_ref == discardable_bitmap[1][0].pixelRef());
    EXPECT_TRUE(pixel_refs[4].pixel_ref == discardable_bitmap[1][1].pixelRef());
    // Bottom right tile with bitmap[1][1].
    EXPECT_TRUE(pixel_refs[5].pixel_ref == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect).ToString());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect).ToString());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect).ToString());
    EXPECT_EQ(translate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[3].pixel_ref_rect).ToString());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[4].pixel_ref_rect).ToString());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[5].pixel_ref_rect).ToString());
  }
  {
    std::vector<skia::PositionPixelRef> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(0, 0, 512, 512), 2.0, &pixel_refs);
    EXPECT_EQ(6u, pixel_refs.size());
    // Top left tile with bitmap[0][0] and bitmap[1][1].
    EXPECT_TRUE(pixel_refs[0].pixel_ref == discardable_bitmap[0][0].pixelRef());
    EXPECT_TRUE(pixel_refs[1].pixel_ref == discardable_bitmap[1][1].pixelRef());
    // Top right tile with bitmap[1][1].
    EXPECT_TRUE(pixel_refs[2].pixel_ref == discardable_bitmap[1][1].pixelRef());
    // Bottom left tile with bitmap[1][0] and bitmap[1][1].
    EXPECT_TRUE(pixel_refs[3].pixel_ref == discardable_bitmap[1][0].pixelRef());
    EXPECT_TRUE(pixel_refs[4].pixel_ref == discardable_bitmap[1][1].pixelRef());
    // Bottom right tile with bitmap[1][1].
    EXPECT_TRUE(pixel_refs[5].pixel_ref == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect).ToString());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect).ToString());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect).ToString());
    EXPECT_EQ(translate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[3].pixel_ref_rect).ToString());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[4].pixel_ref_rect).ToString());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[5].pixel_ref_rect).ToString());
  }
  {
    std::vector<skia::PositionPixelRef> pixel_refs;
    raster_source->GatherPixelRefs(gfx::Rect(0, 0, 128, 128), 0.5, &pixel_refs);
    EXPECT_EQ(6u, pixel_refs.size());
    // Top left tile with bitmap[0][0] and bitmap[1][1].
    EXPECT_TRUE(pixel_refs[0].pixel_ref == discardable_bitmap[0][0].pixelRef());
    EXPECT_TRUE(pixel_refs[1].pixel_ref == discardable_bitmap[1][1].pixelRef());
    // Top right tile with bitmap[1][1].
    EXPECT_TRUE(pixel_refs[2].pixel_ref == discardable_bitmap[1][1].pixelRef());
    // Bottom left tile with bitmap[1][0] and bitmap[1][1].
    EXPECT_TRUE(pixel_refs[3].pixel_ref == discardable_bitmap[1][0].pixelRef());
    EXPECT_TRUE(pixel_refs[4].pixel_ref == discardable_bitmap[1][1].pixelRef());
    // Bottom right tile with bitmap[1][1].
    EXPECT_TRUE(pixel_refs[5].pixel_ref == discardable_bitmap[1][1].pixelRef());
    EXPECT_EQ(rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[0].pixel_ref_rect).ToString());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[1].pixel_ref_rect).ToString());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[2].pixel_ref_rect).ToString());
    EXPECT_EQ(translate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[3].pixel_ref_rect).ToString());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[4].pixel_ref_rect).ToString());
    EXPECT_EQ(rotate_rect.ToString(),
              gfx::SkRectToRectF(pixel_refs[5].pixel_ref_rect).ToString());
  }
}

TEST_F(DisplayListRecordingSourceTest, ExposesEnoughNewAreaEmpty) {
  gfx::Size layer_size(1000, 1000);

  // Both empty means there is nothing to do.
  EXPECT_FALSE(DisplayListRecordingSource::ExposesEnoughNewArea(
      gfx::Rect(), gfx::Rect(), layer_size));
  // Going from empty to non-empty means we must re-record because it could be
  // the first frame after construction or Clear.
  EXPECT_TRUE(DisplayListRecordingSource::ExposesEnoughNewArea(
      gfx::Rect(), gfx::Rect(1, 1), layer_size));

  // Going from non-empty to empty is not special-cased.
  EXPECT_FALSE(DisplayListRecordingSource::ExposesEnoughNewArea(
      gfx::Rect(1, 1), gfx::Rect(), layer_size));
}

TEST_F(DisplayListRecordingSourceTest, ExposesEnoughNewAreaNotBigEnough) {
  gfx::Size layer_size(1000, 1000);
  gfx::Rect current_recorded_viewport(100, 100, 100, 100);
  EXPECT_FALSE(DisplayListRecordingSource::ExposesEnoughNewArea(
      current_recorded_viewport, gfx::Rect(100, 100, 90, 90), layer_size));
  EXPECT_FALSE(DisplayListRecordingSource::ExposesEnoughNewArea(
      current_recorded_viewport, gfx::Rect(100, 100, 100, 100), layer_size));
  EXPECT_FALSE(DisplayListRecordingSource::ExposesEnoughNewArea(
      current_recorded_viewport, gfx::Rect(1, 1, 200, 200), layer_size));
}

TEST_F(DisplayListRecordingSourceTest,
       ExposesEnoughNewAreaNotBigEnoughButNewAreaTouchesEdge) {
  gfx::Size layer_size(500, 500);
  gfx::Rect current_recorded_viewport(100, 100, 100, 100);

  // Top edge.
  EXPECT_TRUE(DisplayListRecordingSource::ExposesEnoughNewArea(
      current_recorded_viewport, gfx::Rect(100, 0, 100, 200), layer_size));

  // Left edge.
  EXPECT_TRUE(DisplayListRecordingSource::ExposesEnoughNewArea(
      current_recorded_viewport, gfx::Rect(0, 100, 200, 100), layer_size));

  // Bottom edge.
  EXPECT_TRUE(DisplayListRecordingSource::ExposesEnoughNewArea(
      current_recorded_viewport, gfx::Rect(100, 100, 100, 400), layer_size));

  // Right edge.
  EXPECT_TRUE(DisplayListRecordingSource::ExposesEnoughNewArea(
      current_recorded_viewport, gfx::Rect(100, 100, 400, 100), layer_size));
}

// Verifies that having a current viewport that touches a layer edge does not
// force re-recording.
TEST_F(DisplayListRecordingSourceTest,
       ExposesEnoughNewAreaCurrentViewportTouchesEdge) {
  gfx::Size layer_size(500, 500);
  gfx::Rect potential_new_viewport(100, 100, 300, 300);

  // Top edge.
  EXPECT_FALSE(DisplayListRecordingSource::ExposesEnoughNewArea(
      gfx::Rect(100, 0, 100, 100), potential_new_viewport, layer_size));

  // Left edge.
  EXPECT_FALSE(DisplayListRecordingSource::ExposesEnoughNewArea(
      gfx::Rect(0, 100, 100, 100), potential_new_viewport, layer_size));

  // Bottom edge.
  EXPECT_FALSE(DisplayListRecordingSource::ExposesEnoughNewArea(
      gfx::Rect(300, 400, 100, 100), potential_new_viewport, layer_size));

  // Right edge.
  EXPECT_FALSE(DisplayListRecordingSource::ExposesEnoughNewArea(
      gfx::Rect(400, 300, 100, 100), potential_new_viewport, layer_size));
}

TEST_F(DisplayListRecordingSourceTest, ExposesEnoughNewAreaScrollScenarios) {
  gfx::Size layer_size(1000, 1000);
  gfx::Rect current_recorded_viewport(100, 100, 100, 100);

  gfx::Rect new_recorded_viewport(current_recorded_viewport);
  new_recorded_viewport.Offset(512, 0);
  EXPECT_FALSE(DisplayListRecordingSource::ExposesEnoughNewArea(
      current_recorded_viewport, new_recorded_viewport, layer_size));
  new_recorded_viewport.Offset(0, 512);
  EXPECT_FALSE(DisplayListRecordingSource::ExposesEnoughNewArea(
      current_recorded_viewport, new_recorded_viewport, layer_size));

  new_recorded_viewport.Offset(1, 0);
  EXPECT_TRUE(DisplayListRecordingSource::ExposesEnoughNewArea(
      current_recorded_viewport, new_recorded_viewport, layer_size));

  new_recorded_viewport.Offset(-1, 1);
  EXPECT_TRUE(DisplayListRecordingSource::ExposesEnoughNewArea(
      current_recorded_viewport, new_recorded_viewport, layer_size));
}

// Verifies that UpdateAndExpandInvalidation calls ExposesEnoughNewArea with the
// right arguments.
TEST_F(DisplayListRecordingSourceTest,
       ExposesEnoughNewAreaCalledWithCorrectArguments) {
  gfx::Size grid_cell_size(128, 128);
  DisplayListRecordingSource recording_source(grid_cell_size);
  FakeContentLayerClient client;
  Region invalidation;
  gfx::Size layer_size(9000, 9000);
  gfx::Rect visible_rect(0, 0, 256, 256);

  recording_source.UpdateAndExpandInvalidation(
      &client, &invalidation, layer_size, visible_rect, 0,
      RecordingSource::RECORD_NORMALLY);
  EXPECT_EQ(gfx::Rect(0, 0, 8256, 8256), recording_source.recorded_viewport());

  visible_rect.Offset(0, 512);
  recording_source.UpdateAndExpandInvalidation(
      &client, &invalidation, layer_size, visible_rect, 0,
      RecordingSource::RECORD_NORMALLY);
  EXPECT_EQ(gfx::Rect(0, 0, 8256, 8256), recording_source.recorded_viewport());

  // Move past the threshold for enough exposed new area.
  visible_rect.Offset(0, 1);
  recording_source.UpdateAndExpandInvalidation(
      &client, &invalidation, layer_size, visible_rect, 0,
      RecordingSource::RECORD_NORMALLY);
  EXPECT_EQ(gfx::Rect(0, 0, 8256, 8769), recording_source.recorded_viewport());

  // Make the bottom of the potential new recorded viewport coincide with the
  // layer's bottom edge.
  visible_rect.Offset(0, 231);
  recording_source.UpdateAndExpandInvalidation(
      &client, &invalidation, layer_size, visible_rect, 0,
      RecordingSource::RECORD_NORMALLY);
  EXPECT_EQ(gfx::Rect(0, 0, 8256, 9000), recording_source.recorded_viewport());
}

}  // namespace
}  // namespace cc
