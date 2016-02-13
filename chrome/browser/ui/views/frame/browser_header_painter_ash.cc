// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_header_painter_ash.h"

#include "ash/ash_layout_constants.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/frame/header_painter_util.h"
#include "base/logging.h"  // DCHECK
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "grit/ash_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using views::Widget;

namespace {
// Color for the window title text.
const SkColor kWindowTitleTextColor = SkColorSetRGB(40, 40, 40);
// Duration of crossfade animation for activating and deactivating frame.
const int kActivationCrossfadeDurationMs = 200;

// Creates a path with rounded top corners.
SkPath MakeRoundRectPath(const gfx::Rect& bounds,
                         int top_left_corner_radius,
                         int top_right_corner_radius) {
  SkRect rect = gfx::RectToSkRect(bounds);
  const SkScalar kTopLeftRadius = SkIntToScalar(top_left_corner_radius);
  const SkScalar kTopRightRadius = SkIntToScalar(top_right_corner_radius);
  SkScalar radii[8] = {
      kTopLeftRadius, kTopLeftRadius,  // top-left
      kTopRightRadius, kTopRightRadius,  // top-right
      0, 0,   // bottom-right
      0, 0};  // bottom-left
  SkPath path;
  path.addRoundRect(rect, radii, SkPath::kCW_Direction);
  return path;
}

// Tiles |frame_image| and |frame_overlay_image| into an area, rounding the top
// corners.
void PaintFrameImagesInRoundRect(gfx::Canvas* canvas,
                                 const gfx::ImageSkia& frame_image,
                                 const gfx::ImageSkia& frame_overlay_image,
                                 const SkPaint& paint,
                                 const gfx::Rect& bounds,
                                 int corner_radius,
                                 int image_inset_x) {
  SkPath frame_path = MakeRoundRectPath(bounds, corner_radius, corner_radius);
  // If |paint| is using an unusual SkXfermode::Mode (this is the case while
  // crossfading), we must create a new canvas to overlay |frame_image| and
  // |frame_overlay_image| using |normal_mode| and then paint the result
  // using the unusual mode. We try to avoid this because creating a new
  // browser-width canvas is expensive.
  SkXfermode::Mode normal_mode;
  SkXfermode::AsMode(nullptr, &normal_mode);
  bool fast_path = (frame_overlay_image.isNull() ||
      SkXfermode::IsMode(paint.getXfermode(), normal_mode));
  if (fast_path) {
    if (frame_image.isNull()) {
      canvas->DrawPath(frame_path, paint);
    } else {
      canvas->DrawImageInPath(frame_image, -image_inset_x, 0, frame_path,
                              paint);
    }

    if (!frame_overlay_image.isNull()) {
      // Adjust |bounds| such that |frame_overlay_image| is not tiled.
      gfx::Rect overlay_bounds = bounds;
      overlay_bounds.Intersect(
          gfx::Rect(bounds.origin(), frame_overlay_image.size()));
      int top_left_corner_radius = corner_radius;
      int top_right_corner_radius = corner_radius;
      if (overlay_bounds.width() < bounds.width() - corner_radius)
        top_right_corner_radius = 0;
      canvas->DrawImageInPath(
          frame_overlay_image, 0, 0,
          MakeRoundRectPath(overlay_bounds, top_left_corner_radius,
                            top_right_corner_radius),
          paint);
    }
  } else {
    gfx::Canvas temporary_canvas(bounds.size(), canvas->image_scale(), false);
    if (frame_image.isNull()) {
      temporary_canvas.DrawColor(paint.getColor());
    } else {
      temporary_canvas.TileImageInt(frame_image, image_inset_x, 0, 0, 0,
                                    bounds.width(), bounds.height());
    }
    temporary_canvas.DrawImageInt(frame_overlay_image, 0, 0);
    canvas->DrawImageInPath(gfx::ImageSkia(temporary_canvas.ExtractImageRep()),
                            0, 0, frame_path, paint);
  }
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserHeaderPainterAsh, public:

BrowserHeaderPainterAsh::BrowserHeaderPainterAsh()
    : frame_(nullptr),
      is_tabbed_(false),
      is_incognito_(false),
      view_(nullptr),
      window_icon_(nullptr),
      caption_button_container_(nullptr),
      painted_height_(0),
      initial_paint_(true),
      mode_(MODE_INACTIVE),
      activation_animation_(new gfx::SlideAnimation(this)) {
}

BrowserHeaderPainterAsh::~BrowserHeaderPainterAsh() {
}

void BrowserHeaderPainterAsh::Init(
    views::Widget* frame,
    BrowserView* browser_view,
    views::View* header_view,
    views::View* window_icon,
    ash::FrameCaptionButtonContainerView* caption_button_container) {
  DCHECK(frame);
  DCHECK(browser_view);
  DCHECK(header_view);
  // window_icon may be null.
  DCHECK(caption_button_container);
  frame_ = frame;

  is_tabbed_ = browser_view->browser()->is_type_tabbed();
  is_incognito_ = !browser_view->IsRegularOrGuestSession();

  view_ = header_view;
  window_icon_ = window_icon;
  caption_button_container_ = caption_button_container;
  // Use light images in otr, even when a custom theme is installed. The
  // otr window with a custom theme is still darker than a normal window.
  caption_button_container_->SetUseLightImages(is_incognito_);
}

int BrowserHeaderPainterAsh::GetMinimumHeaderWidth() const {
  // Ensure we have enough space for the window icon and buttons. We allow
  // the title string to collapse to zero width.
  return GetTitleBounds().x() +
      caption_button_container_->GetMinimumSize().width();
}

void BrowserHeaderPainterAsh::PaintHeader(gfx::Canvas* canvas, Mode mode) {
  Mode old_mode = mode_;
  mode_ = mode;

  if (mode_ != old_mode) {
    if (!initial_paint_ &&
        ash::HeaderPainterUtil::CanAnimateActivation(frame_)) {
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
      0 : ash::HeaderPainterUtil::GetTopCornerRadiusWhenRestored();

  int active_alpha = activation_animation_->CurrentValueBetween(0, 255);
  int inactive_alpha = 255 - active_alpha;

  SkPaint paint;
  if (inactive_alpha > 0) {
    if (active_alpha > 0)
      paint.setXfermodeMode(SkXfermode::kPlus_Mode);

    gfx::ImageSkia inactive_frame_image = GetFrameImage(MODE_INACTIVE);
    gfx::ImageSkia inactive_frame_overlay_image =
        GetFrameOverlayImage(MODE_INACTIVE);

    paint.setAlpha(inactive_alpha);
    paint.setColor(SkColorSetA(GetFrameColor(MODE_INACTIVE), inactive_alpha));
    PaintFrameImagesInRoundRect(
        canvas, inactive_frame_image, inactive_frame_overlay_image, paint,
        GetPaintedBounds(), corner_radius,
        ash::HeaderPainterUtil::GetThemeBackgroundXInset());
  }

  if (active_alpha > 0) {
    gfx::ImageSkia active_frame_image = GetFrameImage(MODE_ACTIVE);
    gfx::ImageSkia active_frame_overlay_image =
        GetFrameOverlayImage(MODE_ACTIVE);

    paint.setAlpha(active_alpha);
    paint.setColor(SkColorSetA(GetFrameColor(MODE_ACTIVE), active_alpha));
    PaintFrameImagesInRoundRect(
        canvas,
        active_frame_image,
        active_frame_overlay_image,
        paint,
        GetPaintedBounds(),
        corner_radius,
        ash::HeaderPainterUtil::GetThemeBackgroundXInset());
  }

  if (!ui::MaterialDesignController::IsModeMaterial() &&
      !frame_->IsMaximized() &&
      !frame_->IsFullscreen()) {
    PaintHighlightForRestoredWindow(canvas);
  }

  if (frame_->widget_delegate() &&
      frame_->widget_delegate()->ShouldShowWindowTitle()) {
    PaintTitleBar(canvas);
  }
}

void BrowserHeaderPainterAsh::LayoutHeader() {
  // Purposefully set |painted_height_| to an invalid value. We cannot use
  // |painted_height_| because the computation of |painted_height_| may depend
  // on having laid out the window controls.
  painted_height_ = -1;

  UpdateCaptionButtons();
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
    gfx::Size icon_size(window_icon_->GetPreferredSize());
    int icon_offset_y = (caption_button_container_->height() -
                         icon_size.height()) / 2;
    window_icon_->SetBounds(ash::HeaderPainterUtil::GetLeftViewXInset(),
                            icon_offset_y,
                            icon_size.width(),
                            icon_size.height());
  }
}

int BrowserHeaderPainterAsh::GetHeaderHeight() const {
  return caption_button_container_->height();
}

int BrowserHeaderPainterAsh::GetHeaderHeightForPainting() const {
  return painted_height_;
}

void BrowserHeaderPainterAsh::SetHeaderHeightForPainting(int height) {
  painted_height_ = height;
}

void BrowserHeaderPainterAsh::SchedulePaintForTitle() {
  view_->SchedulePaintInRect(GetTitleBounds());
}

///////////////////////////////////////////////////////////////////////////////
// gfx::AnimationDelegate overrides:

void BrowserHeaderPainterAsh::AnimationProgressed(
    const gfx::Animation* animation) {
  view_->SchedulePaintInRect(GetPaintedBounds());
}

///////////////////////////////////////////////////////////////////////////////
// BrowserHeaderPainterAsh, private:

void BrowserHeaderPainterAsh::PaintHighlightForRestoredWindow(
    gfx::Canvas* canvas) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia top_left_corner = *rb.GetImageSkiaNamed(
      IDR_ASH_BROWSER_WINDOW_HEADER_SHADE_TOP_LEFT);
  gfx::ImageSkia top_right_corner = *rb.GetImageSkiaNamed(
      IDR_ASH_BROWSER_WINDOW_HEADER_SHADE_TOP_RIGHT);
  gfx::ImageSkia top_edge = *rb.GetImageSkiaNamed(
      IDR_ASH_BROWSER_WINDOW_HEADER_SHADE_TOP);
  gfx::ImageSkia left_edge = *rb.GetImageSkiaNamed(
      IDR_ASH_BROWSER_WINDOW_HEADER_SHADE_LEFT);
  gfx::ImageSkia right_edge = *rb.GetImageSkiaNamed(
      IDR_ASH_BROWSER_WINDOW_HEADER_SHADE_RIGHT);

  int top_left_width = top_left_corner.width();
  int top_left_height = top_left_corner.height();
  canvas->DrawImageInt(top_left_corner, 0, 0);

  int top_right_width = top_right_corner.width();
  int top_right_height = top_right_corner.height();
  canvas->DrawImageInt(top_right_corner,
                       view_->width() - top_right_width,
                       0);

  canvas->TileImageInt(
      top_edge,
      top_left_width,
      0,
      view_->width() - top_left_width - top_right_width,
      top_edge.height());

  canvas->TileImageInt(left_edge,
                       0,
                       top_left_height,
                       left_edge.width(),
                       painted_height_ - top_left_height);

  canvas->TileImageInt(right_edge,
                       view_->width() - right_edge.width(),
                       top_right_height,
                       right_edge.width(),
                       painted_height_ - top_right_height);
}

void BrowserHeaderPainterAsh::PaintTitleBar(gfx::Canvas* canvas) {
  // The window icon is painted by its own views::View.
  gfx::Rect title_bounds = GetTitleBounds();
  title_bounds.set_x(view_->GetMirroredXForRect(title_bounds));
  canvas->DrawStringRectWithFlags(frame_->widget_delegate()->GetWindowTitle(),
                                  BrowserFrame::GetTitleFontList(),
                                  kWindowTitleTextColor,
                                  title_bounds,
                                  gfx::Canvas::NO_SUBPIXEL_RENDERING);
}

gfx::ImageSkia BrowserHeaderPainterAsh::GetFrameImage(Mode mode) const {
  if (!is_tabbed_)
    return gfx::ImageSkia();

  const ui::ThemeProvider* tp = frame_->GetThemeProvider();
  int frame_image_id =
      (mode == MODE_ACTIVE) ? IDR_THEME_FRAME : IDR_THEME_FRAME_INACTIVE;

  // Even if there's no explicit custom image for IDR_THEME_FRAME_INACTIVE, one
  // will be generated for it based on IDR_THEME_FRAME.
  return tp->HasCustomImage(frame_image_id) ||
                 tp->HasCustomImage(IDR_THEME_FRAME)
             ? *tp->GetImageSkiaNamed(frame_image_id)
             : gfx::ImageSkia();
}

gfx::ImageSkia BrowserHeaderPainterAsh::GetFrameOverlayImage(Mode mode) const {
  if (is_incognito_ || !is_tabbed_)
    return gfx::ImageSkia();

  const ui::ThemeProvider* tp = frame_->GetThemeProvider();
  int frame_overlay_image_id = (mode == MODE_ACTIVE)
                                   ? IDR_THEME_FRAME_OVERLAY
                                   : IDR_THEME_FRAME_OVERLAY_INACTIVE;
  return tp->HasCustomImage(frame_overlay_image_id)
             ? *tp->GetImageSkiaNamed(frame_overlay_image_id)
             : gfx::ImageSkia();
}

SkColor BrowserHeaderPainterAsh::GetFrameColor(Mode mode) const {
  int color = (mode == MODE_ACTIVE) ? ThemeProperties::COLOR_FRAME
                                    : ThemeProperties::COLOR_FRAME_INACTIVE;
  // We only theme tabbed windows; fall back to default for non-tabbed.
  return is_tabbed_ ? frame_->GetThemeProvider()->GetColor(color)
                    : ThemeProperties::GetDefaultColor(color, is_incognito_);
}

void BrowserHeaderPainterAsh::UpdateCaptionButtons() {
  caption_button_container_->SetButtonImage(
      ash::CAPTION_BUTTON_ICON_MINIMIZE,
      gfx::VectorIconId::WINDOW_CONTROL_MINIMIZE);
  caption_button_container_->SetButtonImage(
      ash::CAPTION_BUTTON_ICON_CLOSE, gfx::VectorIconId::WINDOW_CONTROL_CLOSE);
  caption_button_container_->SetButtonImage(
      ash::CAPTION_BUTTON_ICON_LEFT_SNAPPED,
      gfx::VectorIconId::WINDOW_CONTROL_LEFT_SNAPPED);
  caption_button_container_->SetButtonImage(
      ash::CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
      gfx::VectorIconId::WINDOW_CONTROL_RIGHT_SNAPPED);

  gfx::VectorIconId size_icon_id = gfx::VectorIconId::WINDOW_CONTROL_MAXIMIZE;
  gfx::Size button_size(
      GetAshLayoutSize(AshLayoutSize::BROWSER_RESTORED_CAPTION_BUTTON));
  if (frame_->IsMaximized() || frame_->IsFullscreen()) {
    size_icon_id = gfx::VectorIconId::WINDOW_CONTROL_RESTORE;
    button_size =
        GetAshLayoutSize(AshLayoutSize::BROWSER_MAXIMIZED_CAPTION_BUTTON);
  }
  caption_button_container_->SetButtonImage(
      ash::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
      size_icon_id);
  caption_button_container_->SetButtonSize(button_size);
}

gfx::Rect BrowserHeaderPainterAsh::GetPaintedBounds() const {
  return gfx::Rect(view_->width(), painted_height_);
}

gfx::Rect BrowserHeaderPainterAsh::GetTitleBounds() const {
  return ash::HeaderPainterUtil::GetTitleBounds(window_icon_,
      caption_button_container_, BrowserFrame::GetTitleFontList());
}
