// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble_border.h"

#include "base/logging.h"
#include "gfx/canvas_skia.h"
#include "gfx/path.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"

// static
SkBitmap* BubbleBorder::left_ = NULL;
SkBitmap* BubbleBorder::top_left_ = NULL;
SkBitmap* BubbleBorder::top_ = NULL;
SkBitmap* BubbleBorder::top_right_ = NULL;
SkBitmap* BubbleBorder::right_ = NULL;
SkBitmap* BubbleBorder::bottom_right_ = NULL;
SkBitmap* BubbleBorder::bottom_ = NULL;
SkBitmap* BubbleBorder::bottom_left_ = NULL;
SkBitmap* BubbleBorder::top_arrow_ = NULL;
SkBitmap* BubbleBorder::bottom_arrow_ = NULL;
SkBitmap* BubbleBorder::left_arrow_ = NULL;
SkBitmap* BubbleBorder::right_arrow_ = NULL;

// static
int BubbleBorder::arrow_offset_;

// The height inside the arrow image, in pixels.
static const int kArrowInteriorHeight = 7;

gfx::Rect BubbleBorder::GetBounds(const gfx::Rect& position_relative_to,
                                  const gfx::Size& contents_size) const {
  // Desired size is size of contents enlarged by the size of the border images.
  gfx::Size border_size(contents_size);
  gfx::Insets insets;
  GetInsets(&insets);
  border_size.Enlarge(insets.left() + insets.right(),
                      insets.top() + insets.bottom());

  // Screen position depends on the arrow location.
  // The arrow should overlap the target by some amount since there is space
  // for shadow between arrow tip and bitmap bounds.
  const int kArrowOverlap = 3;
  int x = position_relative_to.x();
  int y = position_relative_to.y();
  int w = position_relative_to.width();
  int h = position_relative_to.height();
  int arrow_offset = override_arrow_offset_ ? override_arrow_offset_ :
                                              arrow_offset_;

  // Calculate bubble x coordinate.
  switch (arrow_location_) {
    case TOP_LEFT:
    case BOTTOM_LEFT:
      x += w / 2 - arrow_offset;
      break;

    case TOP_RIGHT:
    case BOTTOM_RIGHT:
      x += w / 2 + arrow_offset - border_size.width() + 1;
      break;

    case LEFT_TOP:
    case LEFT_BOTTOM:
      x += w - kArrowOverlap;
      break;

    case RIGHT_TOP:
    case RIGHT_BOTTOM:
      x += kArrowOverlap - border_size.width();
      break;

    case NONE:
    case FLOAT:
      x += w / 2 - border_size.width() / 2;
      break;
  }

  // Calculate bubble y coordinate.
  switch (arrow_location_) {
    case TOP_LEFT:
    case TOP_RIGHT:
      y += h - kArrowOverlap;
      break;

    case BOTTOM_LEFT:
    case BOTTOM_RIGHT:
      y += kArrowOverlap - border_size.height();
      break;

    case LEFT_TOP:
    case RIGHT_TOP:
      y += h / 2 - arrow_offset;
      break;

    case LEFT_BOTTOM:
    case RIGHT_BOTTOM:
      y += h / 2 + arrow_offset - border_size.height() + 1;
      break;

    case NONE:
      y += h;
      break;

    case FLOAT:
      y += h / 2 - border_size.height() / 2;
      break;
  }

  return gfx::Rect(x, y, border_size.width(), border_size.height());
}

void BubbleBorder::GetInsets(gfx::Insets* insets) const {
  int top = top_->height();
  int bottom = bottom_->height();
  int left = left_->width();
  int right = right_->width();
  switch (arrow_location_) {
    case TOP_LEFT:
    case TOP_RIGHT:
      top = std::max(top, top_arrow_->height());
      break;

    case BOTTOM_LEFT:
    case BOTTOM_RIGHT:
      bottom = std::max(bottom, bottom_arrow_->height());
      break;

    case LEFT_TOP:
    case LEFT_BOTTOM:
      left = std::max(left, left_arrow_->width());
      break;

    case RIGHT_TOP:
    case RIGHT_BOTTOM:
      right = std::max(right, right_arrow_->width());
      break;

    case NONE:
    case FLOAT:
      // Nothing to do.
      break;
  }
  insets->Set(top, left, bottom, right);
}

int BubbleBorder::SetArrowOffset(int offset, const gfx::Size& contents_size) {
  gfx::Size border_size(contents_size);
  gfx::Insets insets;
  GetInsets(&insets);
  border_size.Enlarge(insets.left() + insets.right(),
                      insets.top() + insets.bottom());
  offset = std::max(arrow_offset_,
      std::min(offset, (is_arrow_on_horizontal(arrow_location_) ?
          border_size.width() : border_size.height()) - arrow_offset_));
  override_arrow_offset_ = offset;
  return override_arrow_offset_;
}

// static
void BubbleBorder::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    // Load images.
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    left_ = rb.GetBitmapNamed(IDR_BUBBLE_L);
    top_left_ = rb.GetBitmapNamed(IDR_BUBBLE_TL);
    top_ = rb.GetBitmapNamed(IDR_BUBBLE_T);
    top_right_ = rb.GetBitmapNamed(IDR_BUBBLE_TR);
    right_ = rb.GetBitmapNamed(IDR_BUBBLE_R);
    bottom_right_ = rb.GetBitmapNamed(IDR_BUBBLE_BR);
    bottom_ = rb.GetBitmapNamed(IDR_BUBBLE_B);
    bottom_left_ = rb.GetBitmapNamed(IDR_BUBBLE_BL);
    left_arrow_ = rb.GetBitmapNamed(IDR_BUBBLE_L_ARROW);
    top_arrow_ = rb.GetBitmapNamed(IDR_BUBBLE_T_ARROW);
    right_arrow_ = rb.GetBitmapNamed(IDR_BUBBLE_R_ARROW);
    bottom_arrow_ = rb.GetBitmapNamed(IDR_BUBBLE_B_ARROW);

    // Calculate horizontal and vertical insets for arrow by ensuring that
    // the widest arrow and corner images will have enough room to avoid overlap
    int offset_x =
        (std::max(top_arrow_->width(), bottom_arrow_->width()) / 2) +
        std::max(std::max(top_left_->width(), top_right_->width()),
                 std::max(bottom_left_->width(), bottom_right_->width()));
    int offset_y =
        (std::max(left_arrow_->height(), right_arrow_->height()) / 2) +
        std::max(std::max(top_left_->height(), top_right_->height()),
                 std::max(bottom_left_->height(), bottom_right_->height()));
    arrow_offset_ = std::max(offset_x, offset_y);

    initialized = true;
  }
}

void BubbleBorder::Paint(const views::View& view, gfx::Canvas* canvas) const {
  // Convenience shorthand variables.
  const int tl_width = top_left_->width();
  const int tl_height = top_left_->height();
  const int t_height = top_->height();
  const int tr_width = top_right_->width();
  const int tr_height = top_right_->height();
  const int l_width = left_->width();
  const int r_width = right_->width();
  const int br_width = bottom_right_->width();
  const int br_height = bottom_right_->height();
  const int b_height = bottom_->height();
  const int bl_width = bottom_left_->width();
  const int bl_height = bottom_left_->height();

  gfx::Insets insets;
  GetInsets(&insets);
  const int top = insets.top() - t_height;
  const int bottom = view.height() - insets.bottom() + b_height;
  const int left = insets.left() - l_width;
  const int right = view.width() - insets.right() + r_width;
  const int height = bottom - top;
  const int width = right - left;

  // |arrow_offset| is offset of arrow from the begining of the edge.
  int arrow_offset = arrow_offset_;
  if (override_arrow_offset_)
    arrow_offset = override_arrow_offset_;
  else if (is_arrow_on_horizontal(arrow_location_) &&
           !is_arrow_on_left(arrow_location_)) {
    arrow_offset = view.width() - arrow_offset - 1;
  } else if (!is_arrow_on_horizontal(arrow_location_) &&
             !is_arrow_on_top(arrow_location_)) {
    arrow_offset = view.height() - arrow_offset - 1;
  }

  // Left edge.
  if (arrow_location_ == LEFT_TOP || arrow_location_ == LEFT_BOTTOM) {
    int start_y = top + tl_height;
    int before_arrow = arrow_offset - start_y - left_arrow_->height() / 2;
    int after_arrow =
        height - tl_height - bl_height - left_arrow_->height() - before_arrow;
    DrawArrowInterior(canvas,
                      false,
                      left_arrow_->width() - kArrowInteriorHeight,
                      start_y + before_arrow + left_arrow_->height() / 2,
                      kArrowInteriorHeight,
                      left_arrow_->height() / 2 - 1);
    DrawEdgeWithArrow(canvas,
                      false,
                      left_,
                      left_arrow_,
                      left,
                      start_y,
                      before_arrow,
                      after_arrow,
                      left_->width() - left_arrow_->width());
  } else {
    canvas->TileImageInt(*left_, left, top + tl_height, l_width,
                         height - tl_height - bl_height);
  }

  // Top left corner.
  canvas->DrawBitmapInt(*top_left_, left, top);

  // Top edge.
  if (arrow_location_ == TOP_LEFT || arrow_location_ == TOP_RIGHT) {
    int start_x = left + tl_width;
    int before_arrow = arrow_offset - start_x - top_arrow_->width() / 2;
    int after_arrow =
        width - tl_width - tr_width - top_arrow_->width() - before_arrow;
    DrawArrowInterior(canvas,
                      true,
                      start_x + before_arrow + top_arrow_->width() / 2,
                      top_arrow_->height() - kArrowInteriorHeight,
                      1 - top_arrow_->width() / 2,
                      kArrowInteriorHeight);
    DrawEdgeWithArrow(canvas,
                      true,
                      top_,
                      top_arrow_,
                      start_x,
                      top,
                      before_arrow,
                      after_arrow,
                      top_->height() - top_arrow_->height());
  } else {
    canvas->TileImageInt(*top_, left + tl_width, top,
                         width - tl_width - tr_width, t_height);
  }

  // Top right corner.
  canvas->DrawBitmapInt(*top_right_, right - tr_width, top);

  // Right edge.
  if (arrow_location_ == RIGHT_TOP || arrow_location_ == RIGHT_BOTTOM) {
    int start_y = top + tr_height;
    int before_arrow = arrow_offset - start_y - right_arrow_->height() / 2;
    int after_arrow = height - tl_height - bl_height -
        right_arrow_->height() - before_arrow;
    DrawArrowInterior(canvas,
                      false,
                      right - r_width + kArrowInteriorHeight,
                      start_y + before_arrow + right_arrow_->height() / 2,
                      -kArrowInteriorHeight,
                      right_arrow_->height() / 2 - 1);
    DrawEdgeWithArrow(canvas,
                      false,
                      right_,
                      right_arrow_,
                      right - r_width,
                      start_y,
                      before_arrow,
                      after_arrow,
                      0);
  } else {
    canvas->TileImageInt(*right_, right - r_width, top + tr_height, r_width,
                         height - tr_height - br_height);
  }

  // Bottom right corner.
  canvas->DrawBitmapInt(*bottom_right_, right - br_width, bottom - br_height);

  // Bottom edge.
  if (arrow_location_ == BOTTOM_LEFT || arrow_location_ == BOTTOM_RIGHT) {
    int start_x = left + bl_width;
    int before_arrow = arrow_offset - start_x - bottom_arrow_->width() / 2;
    int after_arrow =
        width - bl_width - br_width - bottom_arrow_->width() - before_arrow;
    DrawArrowInterior(canvas,
                      true,
                      start_x + before_arrow + bottom_arrow_->width() / 2,
                      bottom - b_height + kArrowInteriorHeight,
                      1 - bottom_arrow_->width() / 2,
                      -kArrowInteriorHeight);
    DrawEdgeWithArrow(canvas,
                      true,
                      bottom_,
                      bottom_arrow_,
                      start_x,
                      bottom - b_height,
                      before_arrow,
                      after_arrow,
                      0);
  } else {
    canvas->TileImageInt(*bottom_, left + bl_width, bottom - b_height,
                         width - bl_width - br_width, b_height);
  }

  // Bottom left corner.
  canvas->DrawBitmapInt(*bottom_left_, left, bottom - bl_height);
}

void BubbleBorder::DrawEdgeWithArrow(gfx::Canvas* canvas,
                                     bool is_horizontal,
                                     SkBitmap* edge,
                                     SkBitmap* arrow,
                                     int start_x,
                                     int start_y,
                                     int before_arrow,
                                     int after_arrow,
                                     int offset) const {
  /* Here's what the parameters mean:
   *                     start_x
   *                       .
   *                       .        ┌───┐                 ┬ offset
   * start_y..........┌────┬────────┤ ▲ ├────────┬────┐
   *                  │  / │--------│∙ ∙│--------│ \  │
   *                  │ /  ├────────┴───┴────────┤  \ │
   *                  ├───┬┘                     └┬───┤
   *                       └───┬────┘   └───┬────┘
   *             before_arrow ─┘            └─ after_arrow
   */
  if (before_arrow) {
    canvas->TileImageInt(*edge, start_x, start_y,
        is_horizontal ? before_arrow : edge->width(),
        is_horizontal ? edge->height() : before_arrow);
  }

  canvas->DrawBitmapInt(*arrow,
      start_x + (is_horizontal ? before_arrow : offset),
      start_y + (is_horizontal ? offset : before_arrow));

  if (after_arrow) {
    start_x += (is_horizontal ? before_arrow + arrow->width() : 0);
    start_y += (is_horizontal ? 0 : before_arrow + arrow->height());
    canvas->TileImageInt(*edge, start_x, start_y,
        is_horizontal ? after_arrow : edge->width(),
        is_horizontal ? edge->height() : after_arrow);
  }
}

void BubbleBorder::DrawArrowInterior(gfx::Canvas* canvas,
                                     bool is_horizontal,
                                     int tip_x,
                                     int tip_y,
                                     int shift_x,
                                     int shift_y) const {
  /* This function fills the interior of the arrow with background color.
   * It draws isosceles triangle under semitransparent arrow tip.
   *
   * Here's what the parameters mean:
   *
   *    ┌──────── |tip_x|
   * ┌─────┐
   * │  ▲  │ ──── |tip y|
   * │∙∙∙∙∙│ ┐
   * └─────┘ └─── |shift_x| (offset from tip to vertexes of isosceles triangle)
   *  └────────── |shift_y|
   */
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(background_color_);
  gfx::Path path;
  path.incReserve(4);
  path.moveTo(SkIntToScalar(tip_x), SkIntToScalar(tip_y));
  path.lineTo(SkIntToScalar(tip_x + shift_x),
              SkIntToScalar(tip_y + shift_y));
  if (is_horizontal)
    path.lineTo(SkIntToScalar(tip_x - shift_x), SkIntToScalar(tip_y + shift_y));
  else
    path.lineTo(SkIntToScalar(tip_x + shift_x), SkIntToScalar(tip_y - shift_y));
  path.close();
  canvas->AsCanvasSkia()->drawPath(path, paint);
}

/////////////////////////

void BubbleBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  // The border of this view creates an anti-aliased round-rect region for the
  // contents, which we need to fill with the background color.
  // NOTE: This doesn't handle an arrow location of "NONE", which has square top
  // corners.
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(border_->background_color());
  gfx::Path path;
  gfx::Rect bounds(view->GetLocalBounds(false));
  SkRect rect;
  rect.set(SkIntToScalar(bounds.x()), SkIntToScalar(bounds.y()),
           SkIntToScalar(bounds.right()), SkIntToScalar(bounds.bottom()));
  SkScalar radius = SkIntToScalar(BubbleBorder::GetCornerRadius());
  path.addRoundRect(rect, radius, radius);
  canvas->AsCanvasSkia()->drawPath(path, paint);
}
