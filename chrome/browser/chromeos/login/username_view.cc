// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/username_view.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/rounded_view.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkColorShader.h"
#include "third_party/skia/include/core/SkComposeShader.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/rect.h"

namespace chromeos {

namespace {
// Username label background color.
const SkColor kLabelBackgoundColor = 0x55000000;
// Holds margin to height ratio.
const double kMarginRatio = 1.0 / 3.0;
// Holds the frame width for the small shaped username view.
const SkScalar kSmallShapeFrameWidth = SkIntToScalar(1);

// Class that sets up half rounded rectangle (only the bottom corners are
// rounded) as a clip region of the view.
// For more info see the file "chrome/browser/chromeos/login/rounded_view.h".
template<typename C>
class HalfRoundedView : public RoundedView<C> {
 public:
  HalfRoundedView(const std::wstring &text, bool use_small_shape)
      : RoundedView<C>(text, use_small_shape) {
  }

 protected:
  // Overrides ViewFilter.
  virtual SkPath GetClipPath() const {
    if (!C::use_small_shape()) {
      return RoundedView<C>::GetClipPath();
    } else {
      SkPath path;
      gfx::Rect frame_bounds = this->bounds();
      frame_bounds.Inset(kSmallShapeFrameWidth, kSmallShapeFrameWidth,
                   kSmallShapeFrameWidth, kSmallShapeFrameWidth);
      path.addRect(SkIntToScalar(frame_bounds.x()),
                   SkIntToScalar(frame_bounds.y()),
                   SkIntToScalar(frame_bounds.x() + frame_bounds.width()),
                   SkIntToScalar(frame_bounds.y() + frame_bounds.height()));
      return path;
    }
  }

  virtual void DrawFrame(gfx::Canvas* canvas) {
    // No frame is needed.
  }

  virtual SkRect GetViewRect() const {
    SkRect view_rect;
    // The rectangle will be intersected with the bounds, so the correct half
    // of the round rectangle will be obtained.
    view_rect.iset(this->x(),
                   this->y() - this->height(),
                   this->x() + this->width(),
                   this->y() + this->height());
    return view_rect;
  }
};

}  // namespace

UsernameView::UsernameView(const std::wstring& username, bool use_small_shape)
    : views::Label(username.empty()
          ? UTF16ToWide(l10n_util::GetStringUTF16(IDS_GUEST)) : username),
      use_small_shape_(use_small_shape),
      is_guest_(username.empty()) {
}

void UsernameView::Paint(gfx::Canvas* canvas) {
  gfx::Rect bounds = GetContentsBounds();
  if (text_image_ == NULL)
    PaintUsername(bounds);
  DCHECK(text_image_ != NULL);
  DCHECK(bounds.size() ==
         gfx::Size(text_image_->width(), text_image_->height()));
  canvas->DrawBitmapInt(*text_image_, bounds.x(), bounds.y());
}

// static
UsernameView* UsernameView::CreateShapedUsernameView(
    const std::wstring& username, bool use_small_shape) {
  return new HalfRoundedView<UsernameView>(username, use_small_shape);
}

gfx::NativeCursor UsernameView::GetCursorForPoint(
    ui::EventType event_type,
    const gfx::Point& p) {
  return use_small_shape_ ? gfx::GetCursor(GDK_HAND2) : NULL;
}

void UsernameView::PaintUsername(const gfx::Rect& bounds) {
  margin_width_ = bounds.height() * kMarginRatio;
  gfx::CanvasSkia canvas(bounds.width(), bounds.height(), false);
  // Draw transparent background.
  canvas.drawColor(0);

  // Calculate needed space.
  int flags = gfx::Canvas::TEXT_ALIGN_LEFT |
      gfx::Canvas::TEXT_VALIGN_MIDDLE |
      gfx::Canvas::NO_ELLIPSIS;
  int text_height, text_width;
  gfx::CanvasSkia::SizeStringInt(WideToUTF16Hack(GetText()), font(),
                                 &text_width, &text_height,
                                 flags);
  text_width += margin_width_;

  // Also leave the right margin.
  bool use_fading_for_text = text_width + margin_width_ >= bounds.width();

  // Only alpha channel counts.
  SkColor gradient_colors[2];
  gradient_colors[0] = 0xFFFFFFFF;
  gradient_colors[1] = 0x00FFFFFF;

  int gradient_start = use_fading_for_text ?
                       bounds.width() - bounds.height() - margin_width_ :
                       text_width;
  int gradient_end = std::min(gradient_start + bounds.height(),
                              bounds.width() - margin_width_);

  SkPoint gradient_borders[2];
  gradient_borders[0].set(SkIntToScalar(gradient_start), SkIntToScalar(0));
  gradient_borders[1].set(SkIntToScalar(gradient_end), SkIntToScalar(0));

  SkShader* gradient_shader =
      SkGradientShader::CreateLinear(gradient_borders, gradient_colors, NULL, 2,
                                     SkShader::kClamp_TileMode, NULL);

  if (!use_fading_for_text) {
    // Draw the final background with the fading in the end.
    SkShader* solid_shader = new SkColorShader(kLabelBackgoundColor);
    SkXfermode* mode = SkXfermode::Create(SkXfermode::kSrcIn_Mode);
    SkShader* composite_shader = new SkComposeShader(gradient_shader,
                                                     solid_shader, mode);
    gradient_shader->unref();
    solid_shader->unref();

    SkPaint paint;
    paint.setShader(composite_shader)->unref();
    canvas.drawPaint(paint);
  }

  // Draw the text.
  // Note, direct call of the DrawStringInt method produces the green dots
  // along the text perimeter (when the label is place on the white background).
  SkColor kInvisibleHaloColor = 0x00000000;
  canvas.DrawStringWithHalo(WideToUTF16Hack(GetText()), font(), GetColor(),
                            kInvisibleHaloColor, bounds.x() + margin_width_,
                            bounds.y(), bounds.width() - 2 * margin_width_,
                            bounds.height(), flags);

  text_image_.reset(new SkBitmap(canvas.ExtractBitmap()));

  if (use_fading_for_text) {
    // Fade out only the text in the end. Use regular background.
    canvas.drawColor(kLabelBackgoundColor, SkXfermode::kSrc_Mode);
    SkShader* image_shader = SkShader::CreateBitmapShader(
        *text_image_,
        SkShader::kRepeat_TileMode,
        SkShader::kRepeat_TileMode);
    SkXfermode* mode = SkXfermode::Create(SkXfermode::kSrcIn_Mode);
    SkShader* composite_shader = new SkComposeShader(gradient_shader,
                                                     image_shader, mode);
    gradient_shader->unref();
    image_shader->unref();

    SkPaint paint;
    paint.setShader(composite_shader)->unref();
    canvas.drawPaint(paint);
    text_image_.reset(new SkBitmap(canvas.ExtractBitmap()));
  }
}

void UsernameView::OnLocaleChanged() {
  if (is_guest_) {
    SetText(UTF16ToWide(l10n_util::GetStringUTF16(IDS_GUEST)));
    text_image_.reset();
    SchedulePaint();
  }
}

}  // namespace chromeos
