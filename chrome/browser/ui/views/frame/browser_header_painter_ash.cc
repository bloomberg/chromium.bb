// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_header_painter_ash.h"

#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/frame/header_painter_util.h"
#include "base/logging.h"  // DCHECK
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using views::Widget;

namespace {
// Color for the window title text.
const SkColor kWindowTitleTextColor = SkColorSetRGB(40, 40, 40);
// Duration of crossfade animation for activating and deactivating frame.
const int kActivationCrossfadeDurationMs = 200;

// Tiles an image into an area, rounding the top corners. Samples |image|
// starting |image_inset_x| pixels from the left of the image.
void TileRoundRect(gfx::Canvas* canvas,
                   const gfx::ImageSkia& image,
                   const SkPaint& paint,
                   const gfx::Rect& bounds,
                   int top_left_corner_radius,
                   int top_right_corner_radius,
                   int image_inset_x) {
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
  canvas->DrawImageInPath(image, -image_inset_x, 0, path, paint);
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
  SkXfermode::Mode normal_mode;
  SkXfermode::AsMode(NULL, &normal_mode);

  // If |paint| is using an unusual SkXfermode::Mode (this is the case while
  // crossfading), we must create a new canvas to overlay |frame_image| and
  // |frame_overlay_image| using |normal_mode| and then paint the result
  // using the unusual mode. We try to avoid this because creating a new
  // browser-width canvas is expensive.
  bool fast_path = (frame_overlay_image.isNull() ||
      SkXfermode::IsMode(paint.getXfermode(), normal_mode));
  if (fast_path) {
    TileRoundRect(canvas, frame_image, paint, bounds, corner_radius,
        corner_radius, image_inset_x);

    if (!frame_overlay_image.isNull()) {
      // Adjust |bounds| such that |frame_overlay_image| is not tiled.
      gfx::Rect overlay_bounds = bounds;
      overlay_bounds.Intersect(
          gfx::Rect(bounds.origin(), frame_overlay_image.size()));
      int top_left_corner_radius = corner_radius;
      int top_right_corner_radius = corner_radius;
      if (overlay_bounds.width() < bounds.width() - corner_radius)
        top_right_corner_radius = 0;
      TileRoundRect(canvas, frame_overlay_image, paint, overlay_bounds,
          top_left_corner_radius, top_right_corner_radius, 0);
    }
  } else {
    gfx::Canvas temporary_canvas(bounds.size(), canvas->image_scale(), false);
    temporary_canvas.TileImageInt(frame_image,
                                  image_inset_x, 0,
                                  0, 0,
                                  bounds.width(), bounds.height());
    temporary_canvas.DrawImageInt(frame_overlay_image, 0, 0);
    TileRoundRect(canvas, gfx::ImageSkia(temporary_canvas.ExtractImageRep()),
        paint, bounds, corner_radius, corner_radius, 0);
  }
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserHeaderPainterAsh, public:

BrowserHeaderPainterAsh::BrowserHeaderPainterAsh()
    : frame_(NULL),
      is_tabbed_(false),
      is_incognito_(false),
      view_(NULL),
      window_icon_(NULL),
      window_icon_x_inset_(ash::HeaderPainterUtil::GetDefaultLeftViewXInset()),
      caption_button_container_(NULL),
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
  // window_icon may be NULL.
  DCHECK(caption_button_container);
  frame_ = frame;

  is_tabbed_ = browser_view->browser()->is_type_tabbed();
  is_incognito_ = !browser_view->IsRegularOrGuestSession();

  view_ = header_view;
  window_icon_ = window_icon;
  caption_button_container_ = caption_button_container;
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

    gfx::ImageSkia inactive_frame_image;
    gfx::ImageSkia inactive_frame_overlay_image;
    GetFrameImages(MODE_INACTIVE, &inactive_frame_image,
        &inactive_frame_overlay_image);

    paint.setAlpha(inactive_alpha);
    PaintFrameImagesInRoundRect(
        canvas,
        inactive_frame_image,
        inactive_frame_overlay_image,
        paint,
        GetPaintedBounds(),
        corner_radius,
        ash::HeaderPainterUtil::GetThemeBackgroundXInset());
  }

  if (active_alpha > 0) {
    gfx::ImageSkia active_frame_image;
    gfx::ImageSkia active_frame_overlay_image;
    GetFrameImages(MODE_ACTIVE, &active_frame_image,
        &active_frame_overlay_image);

    paint.setAlpha(active_alpha);
    PaintFrameImagesInRoundRect(
        canvas,
        active_frame_image,
        active_frame_overlay_image,
        paint,
        GetPaintedBounds(),
        corner_radius,
        ash::HeaderPainterUtil::GetThemeBackgroundXInset());
  }

  if (!frame_->IsMaximized() && !frame_->IsFullscreen())
    PaintHighlightForRestoredWindow(canvas);
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

  UpdateCaptionButtonImages();
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
    window_icon_->SetBounds(window_icon_x_inset_,
                            icon_offset_y,
                            icon_size.width(),
                            icon_size.height());
  }
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

void BrowserHeaderPainterAsh::UpdateLeftViewXInset(int left_view_x_inset) {
  window_icon_x_inset_ = left_view_x_inset;
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

void BrowserHeaderPainterAsh::GetFrameImages(
    Mode mode,
    gfx::ImageSkia* frame_image,
    gfx::ImageSkia* frame_overlay_image) const {
  if (is_tabbed_) {
    GetFrameImagesForTabbedBrowser(mode, frame_image, frame_overlay_image);
  } else {
    *frame_image = GetFrameImageForNonTabbedBrowser(mode);
    *frame_overlay_image = gfx::ImageSkia();
  }
}

void BrowserHeaderPainterAsh::GetFrameImagesForTabbedBrowser(
    Mode mode,
    gfx::ImageSkia* frame_image,
    gfx::ImageSkia* frame_overlay_image) const {
  int frame_image_id = 0;
  int frame_overlay_image_id = 0;

  ui::ThemeProvider* tp = frame_->GetThemeProvider();
  if (tp->HasCustomImage(IDR_THEME_FRAME_OVERLAY) && !is_incognito_) {
    frame_overlay_image_id = (mode == MODE_ACTIVE) ?
        IDR_THEME_FRAME_OVERLAY : IDR_THEME_FRAME_OVERLAY_INACTIVE;
  }

  if (mode == MODE_ACTIVE) {
    frame_image_id = is_incognito_ ?
        IDR_THEME_FRAME_INCOGNITO : IDR_THEME_FRAME;
  } else {
    frame_image_id = is_incognito_ ?
        IDR_THEME_FRAME_INCOGNITO_INACTIVE : IDR_THEME_FRAME_INACTIVE;
  }

  *frame_image = *tp->GetImageSkiaNamed(frame_image_id);
  *frame_overlay_image = (frame_overlay_image_id == 0) ?
      gfx::ImageSkia() : *tp->GetImageSkiaNamed(frame_overlay_image_id);
}

gfx::ImageSkia BrowserHeaderPainterAsh::GetFrameImageForNonTabbedBrowser(
    Mode mode) const {
  // Request the images from the ResourceBundle (and not from the ThemeProvider)
  // in order to get the default non-themed assets.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (mode == MODE_ACTIVE) {
    return *rb.GetImageSkiaNamed(is_incognito_ ?
        IDR_THEME_FRAME_INCOGNITO : IDR_THEME_FRAME);
  }
  return *rb.GetImageSkiaNamed(is_incognito_ ?
      IDR_THEME_FRAME_INCOGNITO_INACTIVE : IDR_THEME_FRAME_INACTIVE);
}

void BrowserHeaderPainterAsh::UpdateCaptionButtonImages() {
  int hover_background_id = 0;
  int pressed_background_id = 0;
  if (frame_->IsMaximized() || frame_->IsFullscreen()) {
    hover_background_id =
        IDR_ASH_BROWSER_WINDOW_CONTROL_BACKGROUND_MAXIMIZED_H;
    pressed_background_id =
        IDR_ASH_BROWSER_WINDOW_CONTROL_BACKGROUND_MAXIMIZED_P;
  } else {
    hover_background_id =
        IDR_ASH_BROWSER_WINDOW_CONTROL_BACKGROUND_RESTORED_H;
    pressed_background_id =
        IDR_ASH_BROWSER_WINDOW_CONTROL_BACKGROUND_RESTORED_P;
  }
  caption_button_container_->SetButtonImages(
      ash::CAPTION_BUTTON_ICON_MINIMIZE,
      IDR_ASH_BROWSER_WINDOW_CONTROL_ICON_MINIMIZE,
      IDR_ASH_BROWSER_WINDOW_CONTROL_ICON_MINIMIZE,
      hover_background_id,
      pressed_background_id);

  int size_icon_id = 0;
  if (frame_->IsMaximized() || frame_->IsFullscreen())
    size_icon_id = IDR_ASH_BROWSER_WINDOW_CONTROL_ICON_RESTORE;
  else
    size_icon_id = IDR_ASH_BROWSER_WINDOW_CONTROL_ICON_MAXIMIZE;
  caption_button_container_->SetButtonImages(
      ash::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
      size_icon_id,
      size_icon_id,
      hover_background_id,
      pressed_background_id);

  caption_button_container_->SetButtonImages(
      ash::CAPTION_BUTTON_ICON_CLOSE,
      IDR_ASH_BROWSER_WINDOW_CONTROL_ICON_CLOSE,
      IDR_ASH_BROWSER_WINDOW_CONTROL_ICON_CLOSE,
      hover_background_id,
      pressed_background_id);
  caption_button_container_->SetButtonImages(
      ash::CAPTION_BUTTON_ICON_LEFT_SNAPPED,
      IDR_ASH_BROWSER_WINDOW_CONTROL_ICON_LEFT_SNAPPED,
      IDR_ASH_BROWSER_WINDOW_CONTROL_ICON_LEFT_SNAPPED,
      hover_background_id,
      pressed_background_id);
  caption_button_container_->SetButtonImages(
      ash::CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
      IDR_ASH_BROWSER_WINDOW_CONTROL_ICON_RIGHT_SNAPPED,
      IDR_ASH_BROWSER_WINDOW_CONTROL_ICON_RIGHT_SNAPPED,
      hover_background_id,
      pressed_background_id);
}

gfx::Rect BrowserHeaderPainterAsh::GetPaintedBounds() const {
  return gfx::Rect(view_->width(), painted_height_);
}

gfx::Rect BrowserHeaderPainterAsh::GetTitleBounds() const {
  return ash::HeaderPainterUtil::GetTitleBounds(window_icon_,
      caption_button_container_, BrowserFrame::GetTitleFontList());
}
