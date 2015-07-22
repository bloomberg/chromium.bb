// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/throbber_layer.h"

#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/playback/display_item_list.h"
#include "cc/playback/drawing_display_item.h"
#include "content/public/browser/android/compositor.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/gfx/skia_util.h"

namespace chrome {
namespace android {

// static
scoped_refptr<ThrobberLayer> ThrobberLayer::Create(SkColor color) {
  return make_scoped_refptr(new ThrobberLayer(color));
}

void ThrobberLayer::Show(const base::Time& now) {
  if (!layer_->hide_layer_and_subtree())
    return;
  start_time_ = now;
  layer_->SetHideLayerAndSubtree(false);
  UpdateThrobber(now);
}

void ThrobberLayer::Hide() {
  layer_->SetHideLayerAndSubtree(true);
}

void ThrobberLayer::SetColor(SkColor color) {
  color_ = color;
}

void ThrobberLayer::UpdateThrobber(const base::Time& now) {
  update_time_ = now;
  layer_->SetNeedsDisplay();
}

scoped_refptr<cc::Layer> ThrobberLayer::layer() {
  return layer_;
}

void ThrobberLayer::PaintContents(
    SkCanvas* canvas,
    const gfx::Rect& clip,
    ContentLayerClient::PaintingControlSetting painting_control) {
  // ContentLayerClient::PaintContents() should not be called as we enabled
  // slimming paint on Android. This is kept just in case we revert that change.
  gfx::Rect rect(layer_->bounds());
  canvas->clipRect(RectToSkRect(rect));
  canvas->clear(SK_ColorTRANSPARENT);
  gfx::Canvas gfx_canvas(canvas, 1.0f);
  gfx::PaintThrobberSpinning(&gfx_canvas, rect, color_,
                             update_time_ - start_time_);
}

scoped_refptr<cc::DisplayItemList> ThrobberLayer::PaintContentsToDisplayList(
    const gfx::Rect& clip,
    ContentLayerClient::PaintingControlSetting painting_control) {
  const bool use_cached_picture = true;
  scoped_refptr<cc::DisplayItemList> display_list =
      cc::DisplayItemList::Create(clip, use_cached_picture);

  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(gfx::RectToSkRect(clip));
  PaintContents(canvas, clip, painting_control);

  skia::RefPtr<SkPicture> picture =
      skia::AdoptRef(recorder.endRecordingAsPicture());
  auto* item = display_list->CreateAndAppendItem<cc::DrawingDisplayItem>();
  item->SetNew(picture.Pass());

  display_list->Finalize();
  return display_list;
}

bool ThrobberLayer::FillsBoundsCompletely() const {
  return false;
}

size_t ThrobberLayer::GetApproximateUnsharedMemoryUsage() const {
  return 0;
}

ThrobberLayer::ThrobberLayer(SkColor color)
    : color_(color),
      start_time_(base::Time::Now()),
      layer_(cc::PictureLayer::Create(content::Compositor::LayerSettings(),
                                      this)) {
  layer_->SetIsDrawable(true);
  layer_->SetHideLayerAndSubtree(true);
}

ThrobberLayer::~ThrobberLayer() {
}

}  //  namespace android
}  //  namespace chrome
