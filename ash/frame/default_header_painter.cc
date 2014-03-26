// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/default_header_painter.h"

#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/frame/header_painter_util.h"
#include "base/debug/leak_annotations.h"
#include "base/logging.h"  // DCHECK
#include "grit/ash_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using views::Widget;

namespace {

// Color for the window title text.
const SkColor kTitleTextColor = SkColorSetRGB(40, 40, 40);
// Color of the active window header/content separator line.
const SkColor kHeaderContentSeparatorColor = SkColorSetRGB(150, 150, 152);
// Color of the inactive window header/content separator line.
const SkColor kHeaderContentSeparatorInactiveColor =
    SkColorSetRGB(180, 180, 182);
// Duration of crossfade animation for activating and deactivating frame.
const int kActivationCrossfadeDurationMs = 200;

// Tiles an image into an area, rounding the top corners.
void TileRoundRect(gfx::Canvas* canvas,
                   const gfx::ImageSkia& image,
                   const SkPaint& paint,
                   const gfx::Rect& bounds,
                   int corner_radius) {
  SkRect rect = gfx::RectToSkRect(bounds);
  const SkScalar corner_radius_scalar = SkIntToScalar(corner_radius);
  SkScalar radii[8] = {
      corner_radius_scalar, corner_radius_scalar,  // top-left
      corner_radius_scalar, corner_radius_scalar,  // top-right
      0, 0,   // bottom-right
      0, 0};  // bottom-left
  SkPath path;
  path.addRoundRect(rect, radii, SkPath::kCW_Direction);
  canvas->DrawImageInPath(image, 0, 0, path, paint);
}

// Returns the FontList to use for the title.
const gfx::FontList& GetTitleFontList() {
  static const gfx::FontList* title_font_list =
      new gfx::FontList(views::NativeWidgetAura::GetWindowTitleFontList());
  ANNOTATE_LEAKING_OBJECT_PTR(title_font_list);
  return *title_font_list;
}

}  // namespace

namespace ash {

///////////////////////////////////////////////////////////////////////////////
// DefaultHeaderPainter, public:

DefaultHeaderPainter::DefaultHeaderPainter()
    : frame_(NULL),
      view_(NULL),
      window_icon_(NULL),
      window_icon_size_(HeaderPainterUtil::GetDefaultIconSize()),
      caption_button_container_(NULL),
      height_(0),
      mode_(MODE_INACTIVE),
      initial_paint_(true),
      activation_animation_(new gfx::SlideAnimation(this)) {
}

DefaultHeaderPainter::~DefaultHeaderPainter() {
}

void DefaultHeaderPainter::Init(
    views::Widget* frame,
    views::View* header_view,
    views::View* window_icon,
    FrameCaptionButtonContainerView* caption_button_container) {
  DCHECK(frame);
  DCHECK(header_view);
  // window_icon may be NULL.
  DCHECK(caption_button_container);
  frame_ = frame;
  view_ = header_view;
  window_icon_ = window_icon;
  caption_button_container_ = caption_button_container;

  caption_button_container_->SetButtonImages(
      CAPTION_BUTTON_ICON_MINIMIZE,
      IDR_AURA_WINDOW_CONTROL_ICON_MINIMIZE,
      IDR_AURA_WINDOW_CONTROL_ICON_MINIMIZE_I,
      IDR_AURA_WINDOW_CONTROL_BACKGROUND_H,
      IDR_AURA_WINDOW_CONTROL_BACKGROUND_P);
  caption_button_container_->SetButtonImages(
      CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
      IDR_AURA_WINDOW_CONTROL_ICON_SIZE,
      IDR_AURA_WINDOW_CONTROL_ICON_SIZE_I,
      IDR_AURA_WINDOW_CONTROL_BACKGROUND_H,
      IDR_AURA_WINDOW_CONTROL_BACKGROUND_P);
  caption_button_container_->SetButtonImages(
      CAPTION_BUTTON_ICON_CLOSE,
      IDR_AURA_WINDOW_CONTROL_ICON_CLOSE,
      IDR_AURA_WINDOW_CONTROL_ICON_CLOSE_I,
      IDR_AURA_WINDOW_CONTROL_BACKGROUND_H,
      IDR_AURA_WINDOW_CONTROL_BACKGROUND_P);

  // There is no dedicated icon for the snap-left and snap-right buttons
  // when |frame_| is inactive because they should never be visible while
  // |frame_| is inactive.
  caption_button_container_->SetButtonImages(
      CAPTION_BUTTON_ICON_LEFT_SNAPPED,
      IDR_AURA_WINDOW_CONTROL_ICON_LEFT_SNAPPED,
      IDR_AURA_WINDOW_CONTROL_ICON_LEFT_SNAPPED,
      IDR_AURA_WINDOW_CONTROL_BACKGROUND_H,
      IDR_AURA_WINDOW_CONTROL_BACKGROUND_P);
  caption_button_container_->SetButtonImages(
      CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
      IDR_AURA_WINDOW_CONTROL_ICON_RIGHT_SNAPPED,
      IDR_AURA_WINDOW_CONTROL_ICON_RIGHT_SNAPPED,
      IDR_AURA_WINDOW_CONTROL_BACKGROUND_H,
      IDR_AURA_WINDOW_CONTROL_BACKGROUND_P);
}

int DefaultHeaderPainter::GetMinimumHeaderWidth() const {
  // Ensure we have enough space for the window icon and buttons. We allow
  // the title string to collapse to zero width.
  return GetTitleBounds().x() +
      caption_button_container_->GetMinimumSize().width();
}

void DefaultHeaderPainter::PaintHeader(gfx::Canvas* canvas, Mode mode) {
  Mode old_mode = mode_;
  mode_ = mode;

  if (mode_ != old_mode) {
    if (!initial_paint_ && HeaderPainterUtil::CanAnimateActivation(frame_)) {
      activation_animation_->SetSlideDuration(kActivationCrossfadeDurationMs);
      if (mode_ == MODE_ACTIVE)
        activation_animation_->Show();
      else
        activation_animation_->Hide();
    } else {
      if (mode_ == MODE_ACTIVE)
        activation_animation_->Reset(1);
      else
        activation_animation_->Reset(0);
    }
    initial_paint_ = false;
  }

  int corner_radius = (frame_->IsMaximized() || frame_->IsFullscreen()) ?
      0 : HeaderPainterUtil::GetTopCornerRadiusWhenRestored();

  int active_alpha = activation_animation_->CurrentValueBetween(0, 255);
  int inactive_alpha = 255 - active_alpha;

  SkPaint paint;
  if (inactive_alpha > 0) {
    if (active_alpha > 0)
      paint.setXfermodeMode(SkXfermode::kPlus_Mode);

    paint.setAlpha(inactive_alpha);
    gfx::ImageSkia inactive_frame = *GetInactiveFrameImage();
    TileRoundRect(canvas, inactive_frame, paint, GetLocalBounds(),
        corner_radius);
  }

  if (active_alpha > 0) {
    paint.setAlpha(active_alpha);
    gfx::ImageSkia active_frame = *GetActiveFrameImage();
    TileRoundRect(canvas, active_frame, paint, GetLocalBounds(),
        corner_radius);
  }

  if (!frame_->IsMaximized() &&
      !frame_->IsFullscreen() &&
      mode_ == MODE_INACTIVE) {
    PaintHighlightForInactiveRestoredWindow(canvas);
  }
  if (frame_->widget_delegate() &&
      frame_->widget_delegate()->ShouldShowWindowTitle()) {
    PaintTitleBar(canvas);
  }
  PaintHeaderContentSeparator(canvas);
}

void DefaultHeaderPainter::LayoutHeader() {
  caption_button_container_->Layout();

  gfx::Size caption_button_container_size =
      caption_button_container_->GetPreferredSize();
  caption_button_container_->SetBounds(
      view_->width() - caption_button_container_size.width(),
      0,
      caption_button_container_size.width(),
      caption_button_container_size.height());

  if (window_icon_) {
    // Vertically center the window icon with respect to the caption button
    // container.
    // Floor when computing the center of |caption_button_container_|.
    int icon_offset_y =
        caption_button_container_->height() / 2 - window_icon_size_ / 2;
    window_icon_->SetBounds(HeaderPainterUtil::GetIconXOffset(), icon_offset_y,
                            window_icon_size_, window_icon_size_);
  }

  // The header/content separator line overlays the caption buttons.
  SetHeaderHeightForPainting(caption_button_container_->height());
}

int DefaultHeaderPainter::GetHeaderHeightForPainting() const {
  return height_;
}

void DefaultHeaderPainter::SetHeaderHeightForPainting(int height) {
  height_ = height;
}

void DefaultHeaderPainter::SchedulePaintForTitle() {
  view_->SchedulePaintInRect(GetTitleBounds());
}

void DefaultHeaderPainter::UpdateWindowIcon(views::View* window_icon,
                                            int window_icon_size) {
  window_icon_ = window_icon;
  window_icon_size_ = window_icon_size;
}

///////////////////////////////////////////////////////////////////////////////
// gfx::AnimationDelegate overrides:

void DefaultHeaderPainter::AnimationProgressed(
    const gfx::Animation* animation) {
  view_->SchedulePaintInRect(GetLocalBounds());
}

///////////////////////////////////////////////////////////////////////////////
// DefaultHeaderPainter, private:

void DefaultHeaderPainter::PaintHighlightForInactiveRestoredWindow(
    gfx::Canvas* canvas) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia top_edge = *rb.GetImageSkiaNamed(
      IDR_AURA_WINDOW_HEADER_SHADE_INACTIVE_TOP);
  gfx::ImageSkia left_edge = *rb.GetImageSkiaNamed(
      IDR_AURA_WINDOW_HEADER_SHADE_INACTIVE_LEFT);
  gfx::ImageSkia right_edge = *rb.GetImageSkiaNamed(
      IDR_AURA_WINDOW_HEADER_SHADE_INACTIVE_RIGHT);
  gfx::ImageSkia bottom_edge = *rb.GetImageSkiaNamed(
      IDR_AURA_WINDOW_HEADER_SHADE_INACTIVE_BOTTOM);

  int left_edge_width = left_edge.width();
  int right_edge_width = right_edge.width();
  canvas->DrawImageInt(left_edge, 0, 0);
  canvas->DrawImageInt(right_edge, view_->width() - right_edge_width, 0);
  canvas->TileImageInt(
      top_edge,
      left_edge_width,
      0,
      view_->width() - left_edge_width - right_edge_width,
      top_edge.height());

  DCHECK_EQ(left_edge.height(), right_edge.height());
  int bottom = left_edge.height();
  int bottom_height = bottom_edge.height();
  canvas->TileImageInt(
      bottom_edge,
      left_edge_width,
      bottom - bottom_height,
      view_->width() - left_edge_width - right_edge_width,
      bottom_height);
}

void DefaultHeaderPainter::PaintTitleBar(gfx::Canvas* canvas) {
  // The window icon is painted by its own views::View.
  gfx::Rect title_bounds = GetTitleBounds();
  title_bounds.set_x(view_->GetMirroredXForRect(title_bounds));
  canvas->DrawStringRectWithFlags(frame_->widget_delegate()->GetWindowTitle(),
                                  GetTitleFontList(),
                                  kTitleTextColor,
                                  title_bounds,
                                  gfx::Canvas::NO_SUBPIXEL_RENDERING);
}

void DefaultHeaderPainter::PaintHeaderContentSeparator(gfx::Canvas* canvas) {
  SkColor color = (mode_ == MODE_ACTIVE) ?
      kHeaderContentSeparatorColor :
      kHeaderContentSeparatorInactiveColor;

  SkPaint paint;
  paint.setColor(color);
  // Draw the line as 1px thick regardless of scale factor.
  paint.setStrokeWidth(0);

  float thickness = 1 / canvas->image_scale();
  SkScalar y = SkIntToScalar(height_) - SkFloatToScalar(thickness);
  canvas->sk_canvas()->drawLine(0, y, SkIntToScalar(view_->width()), y, paint);
}

gfx::Rect DefaultHeaderPainter::GetLocalBounds() const {
  return gfx::Rect(view_->width(), height_);
}

gfx::Rect DefaultHeaderPainter::GetTitleBounds() const {
  return HeaderPainterUtil::GetTitleBounds(
      window_icon_, caption_button_container_, GetTitleFontList());
}

gfx::ImageSkia* DefaultHeaderPainter::GetActiveFrameImage() const {
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_AURA_WINDOW_HEADER_BASE);
}

gfx::ImageSkia* DefaultHeaderPainter::GetInactiveFrameImage() const {
  int frame_image_id = (frame_->IsMaximized() || frame_->IsFullscreen()) ?
      IDR_AURA_WINDOW_HEADER_BASE :
      IDR_AURA_WINDOW_HEADER_BASE_RESTORED_INACTIVE;
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      frame_image_id);
}

}  // namespace ash
