// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/vector_icon.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr {

class VectorIconTexture : public UiTexture {
 public:
  VectorIconTexture() {}
  ~VectorIconTexture() override {}

  void SetColor(SkColor color) {
    if (color == color_)
      return;
    color_ = color;
    set_dirty();
  }

  void SetIcon(const gfx::VectorIcon& icon) {
    if (icon_no_1x_.path == icon.path)
      return;
    icon_no_1x_.path = icon.path;
    set_dirty();
  }

 private:
  gfx::Size GetPreferredTextureSize(int width) const override {
    return gfx::Size(width, width);
  }

  gfx::SizeF GetDrawnSize() const override { return size_; }

  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override {
    if (icon_no_1x_.is_empty())
      return;
    cc::SkiaPaintCanvas paint_canvas(sk_canvas);
    gfx::Canvas gfx_canvas(&paint_canvas, 1.0f);

    size_.set_height(texture_size.height());
    size_.set_width(texture_size.width());

    float icon_size = size_.height();
    float icon_corner_offset = (size_.height() - icon_size) / 2;
    VectorIcon::DrawVectorIcon(
        &gfx_canvas, icon_no_1x_, icon_size,
        gfx::PointF(icon_corner_offset, icon_corner_offset), color_);
  }

  gfx::SizeF size_;
  gfx::VectorIcon icon_no_1x_{nullptr, nullptr};
  SkColor color_ = SK_ColorWHITE;
  DISALLOW_COPY_AND_ASSIGN(VectorIconTexture);
};

VectorIcon::VectorIcon(int maximum_width_pixels)
    : TexturedElement(maximum_width_pixels),
      texture_(base::MakeUnique<VectorIconTexture>()) {}
VectorIcon::~VectorIcon() {}

void VectorIcon::SetColor(SkColor color) {
  texture_->SetColor(color);
}

void VectorIcon::SetIcon(const gfx::VectorIcon& icon) {
  texture_->SetIcon(icon);
}

UiTexture* VectorIcon::GetTexture() const {
  return texture_.get();
}

void VectorIcon::DrawVectorIcon(gfx::Canvas* canvas,
                                const gfx::VectorIcon& icon,
                                float size_px,
                                const gfx::PointF& corner,
                                SkColor color) {
  gfx::ScopedCanvas scoped(canvas);
  canvas->Translate(
      {static_cast<int>(corner.x()), static_cast<int>(corner.y())});

  // Explicitly cut out the 1x version of the icon, as PaintVectorIcon draws the
  // 1x version if device scale factor isn't set. See crbug.com/749146. If all
  // icons end up being drawn via VectorIcon instances, this will not be
  // required (the 1x version is automatically elided by this class).
  gfx::VectorIcon icon_no_1x{icon.path, nullptr};
  PaintVectorIcon(canvas, icon_no_1x, size_px, color);
}

}  // namespace vr
