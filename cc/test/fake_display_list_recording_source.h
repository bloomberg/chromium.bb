// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_DISPLAY_LIST_RECORDING_SOURCE_H_
#define CC_TEST_FAKE_DISPLAY_LIST_RECORDING_SOURCE_H_

#include "cc/resources/display_list_recording_source.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/impl_side_painting_settings.h"

namespace cc {

// This class provides method for test to add bitmap and draw rect to content
// layer client. This class also provides function to rerecord to generate a new
// display list.
class FakeDisplayListRecordingSource : public DisplayListRecordingSource {
 public:
  FakeDisplayListRecordingSource(const gfx::Size& grid_cell_size,
                                 bool use_cached_picture)
      : DisplayListRecordingSource(grid_cell_size, use_cached_picture) {}
  ~FakeDisplayListRecordingSource() override {}

  static scoped_ptr<FakeDisplayListRecordingSource> CreateRecordingSource(
      const gfx::Rect& recorded_viewport) {
    scoped_ptr<FakeDisplayListRecordingSource> recording_source(
        new FakeDisplayListRecordingSource(
            ImplSidePaintingSettings().default_tile_grid_size,
            ImplSidePaintingSettings().use_cached_picture_in_display_list));
    recording_source->SetRecordedViewport(recorded_viewport);
    return recording_source;
  }

  void SetRecordedViewport(const gfx::Rect& recorded_viewport) {
    recorded_viewport_ = recorded_viewport;
  }

  void SetGridCellSize(const gfx::Size& grid_cell_size) {
    grid_cell_size_ = grid_cell_size;
  }

  void Rerecord() {
    ContentLayerClient::PaintingControlSetting painting_control =
        ContentLayerClient::PAINTING_BEHAVIOR_NORMAL;
    bool use_cached_picture = true;
    display_list_ =
        DisplayItemList::Create(recorded_viewport_, use_cached_picture);
    client_.PaintContentsToDisplayList(display_list_.get(), recorded_viewport_,
                                       painting_control);
    display_list_->ProcessAppendedItems();
    display_list_->CreateAndCacheSkPicture();
    if (gather_pixel_refs_)
      display_list_->GatherPixelRefs(grid_cell_size_);
  }

  void add_draw_rect(const gfx::RectF& rect) {
    client_.add_draw_rect(rect, default_paint_);
  }

  void add_draw_rect_with_paint(const gfx::RectF& rect, const SkPaint& paint) {
    client_.add_draw_rect(rect, paint);
  }

  void add_draw_bitmap(const SkBitmap& bitmap, const gfx::Point& point) {
    client_.add_draw_bitmap(bitmap, point, default_paint_);
  }

  void add_draw_bitmap_with_transform(const SkBitmap& bitmap,
                                      const gfx::Transform& transform) {
    client_.add_draw_bitmap_with_transform(bitmap, transform, default_paint_);
  }

  void add_draw_bitmap_with_paint(const SkBitmap& bitmap,
                                  const gfx::Point& point,
                                  const SkPaint& paint) {
    client_.add_draw_bitmap(bitmap, point, paint);
  }

  void add_draw_bitmap_with_paint_and_transform(const SkBitmap& bitmap,
                                                const gfx::Transform& transform,
                                                const SkPaint& paint) {
    client_.add_draw_bitmap_with_transform(bitmap, transform, paint);
  }

  void set_default_paint(const SkPaint& paint) { default_paint_ = paint; }

 private:
  FakeContentLayerClient client_;
  SkPaint default_paint_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_DISPLAY_LIST_RECORDING_SOURCE_H_
