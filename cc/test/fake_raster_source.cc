// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_raster_source.h"

#include <limits>

#include "base/synchronization/waitable_event.h"
#include "cc/paint/paint_flags.h"
#include "cc/test/fake_recording_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

scoped_refptr<FakeRasterSource> FakeRasterSource::CreateInfiniteFilled() {
  gfx::Size size(std::numeric_limits<int>::max() / 10,
                 std::numeric_limits<int>::max() / 10);
  return CreateFilled(size);
}

scoped_refptr<FakeRasterSource> FakeRasterSource::CreateFilled(
    const gfx::Size& size) {
  auto recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(size);

  PaintFlags red_flags;
  red_flags.setColor(SK_ColorRED);
  recording_source->add_draw_rect_with_flags(gfx::Rect(size), red_flags);

  PaintFlags salmon_pink_flags;
  salmon_pink_flags.setColor(SK_ColorRED);
  salmon_pink_flags.setAlpha(128);
  recording_source->add_draw_rect_with_flags(gfx::Rect(size),
                                             salmon_pink_flags);

  recording_source->Rerecord();

  return make_scoped_refptr(
      new FakeRasterSource(recording_source.get(), false));
}

scoped_refptr<FakeRasterSource> FakeRasterSource::CreateFilledLCD(
    const gfx::Size& size) {
  auto recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(size);

  PaintFlags red_flags;
  red_flags.setColor(SK_ColorRED);
  recording_source->add_draw_rect_with_flags(gfx::Rect(size), red_flags);

  gfx::Size smaller_size(size.width() - 10, size.height() - 10);
  PaintFlags green_flags;
  green_flags.setColor(SK_ColorGREEN);
  recording_source->add_draw_rect_with_flags(gfx::Rect(smaller_size),
                                             green_flags);

  recording_source->Rerecord();

  return make_scoped_refptr(new FakeRasterSource(recording_source.get(), true));
}

scoped_refptr<FakeRasterSource> FakeRasterSource::CreateFilledSolidColor(
    const gfx::Size& size) {
  auto recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(size);

  PaintFlags red_flags;
  red_flags.setColor(SK_ColorRED);
  recording_source->add_draw_rect_with_flags(gfx::Rect(size), red_flags);
  recording_source->Rerecord();
  auto raster_source =
      make_scoped_refptr(new FakeRasterSource(recording_source.get(), false));
  if (!raster_source->IsSolidColor())
    ADD_FAILURE() << "Not solid color!";
  return raster_source;
}

scoped_refptr<FakeRasterSource> FakeRasterSource::CreatePartiallyFilled(
    const gfx::Size& size,
    const gfx::Rect& recorded_viewport) {
  DCHECK(recorded_viewport.IsEmpty() ||
         gfx::Rect(size).Contains(recorded_viewport));
  auto recording_source =
      FakeRecordingSource::CreateRecordingSource(recorded_viewport, size);

  PaintFlags red_flags;
  red_flags.setColor(SK_ColorRED);
  recording_source->add_draw_rect_with_flags(gfx::Rect(size), red_flags);

  gfx::Size smaller_size(size.width() - 10, size.height() - 10);
  PaintFlags green_flags;
  green_flags.setColor(SK_ColorGREEN);
  recording_source->add_draw_rect_with_flags(gfx::Rect(smaller_size),
                                             green_flags);

  recording_source->Rerecord();
  recording_source->SetRecordedViewport(recorded_viewport);

  return make_scoped_refptr(
      new FakeRasterSource(recording_source.get(), false));
}

scoped_refptr<FakeRasterSource> FakeRasterSource::CreateEmpty(
    const gfx::Size& size) {
  auto recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(size);
  return make_scoped_refptr(
      new FakeRasterSource(recording_source.get(), false));
}

scoped_refptr<FakeRasterSource> FakeRasterSource::CreateFromRecordingSource(
    const RecordingSource* recording_source,
    bool can_use_lcd) {
  return make_scoped_refptr(
      new FakeRasterSource(recording_source, can_use_lcd));
}

scoped_refptr<FakeRasterSource>
FakeRasterSource::CreateFromRecordingSourceWithWaitable(
    const RecordingSource* recording_source,
    bool can_use_lcd,
    base::WaitableEvent* playback_allowed_event) {
  return make_scoped_refptr(new FakeRasterSource(recording_source, can_use_lcd,
                                                 playback_allowed_event));
}

FakeRasterSource::FakeRasterSource(const RecordingSource* recording_source,
                                   bool can_use_lcd)
    : RasterSource(recording_source, can_use_lcd),
      playback_allowed_event_(nullptr) {}

FakeRasterSource::FakeRasterSource(const RecordingSource* recording_source,
                                   bool can_use_lcd,
                                   base::WaitableEvent* playback_allowed_event)
    : RasterSource(recording_source, can_use_lcd),
      playback_allowed_event_(playback_allowed_event) {}

FakeRasterSource::~FakeRasterSource() {}

void FakeRasterSource::PlaybackToCanvas(
    SkCanvas* canvas,
    const PlaybackSettings& settings) const {
  if (playback_allowed_event_)
    playback_allowed_event_->Wait();
  RasterSource::PlaybackToCanvas(canvas, settings);
}

}  // namespace cc
