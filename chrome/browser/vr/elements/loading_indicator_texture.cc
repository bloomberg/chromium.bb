// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/loading_indicator_texture.h"

#include "base/logging.h"
#include "cc/paint/skia_paint_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace vr {

namespace {

static constexpr float kWidth = 0.24;
static constexpr float kHeight = 0.008;

}  // namespace

LoadingIndicatorTexture::LoadingIndicatorTexture() = default;

LoadingIndicatorTexture::~LoadingIndicatorTexture() = default;

gfx::Size LoadingIndicatorTexture::GetPreferredTextureSize(
    int maximum_width) const {
  return gfx::Size(maximum_width, maximum_width * kHeight / kWidth);
}

gfx::SizeF LoadingIndicatorTexture::GetDrawnSize() const {
  return size_;
}

void LoadingIndicatorTexture::SetLoading(bool loading) {
  if (loading_ != loading) {
    loading_ = loading;
    set_dirty();
  }
}

void LoadingIndicatorTexture::SetLoadProgress(float progress) {
  DCHECK_GE(progress, 0.0f);
  DCHECK_LE(progress, 1.0f);
  if (progress_ != progress) {
    progress_ = progress;
    set_dirty();
  }
}

void LoadingIndicatorTexture::Draw(SkCanvas* canvas,
                                   const gfx::Size& texture_size) {
  size_.set_height(texture_size.height());
  size_.set_width(texture_size.width());

  canvas->save();
  canvas->scale(size_.width() / kWidth, size_.width() / kWidth);

  SkPaint paint;
  paint.setColor(color_scheme().loading_indicator_background);
  canvas->drawRoundRect({0, 0, kWidth, kHeight}, kHeight / 2, kHeight / 2,
                        paint);

  if (loading_) {
    paint.setColor(color_scheme().loading_indicator_foreground);
    float progress_width = kHeight + (kWidth - kHeight) * progress_;
    canvas->drawRoundRect({0, 0, progress_width, kHeight}, kHeight / 2,
                          kHeight / 2, paint);
  }

  canvas->restore();
}

}  // namespace vr
