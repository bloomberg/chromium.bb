// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_header_painter_ash.h"

#include "ash/common/ash_layout_constants.h"
#include "ash/common/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/common/frame/header_painter_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/logging.h"  // DCHECK
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using views::Widget;

namespace {

// Color for the window title text.
const SkColor kNormalWindowTitleTextColor = SkColorSetRGB(40, 40, 40);
const SkColor kIncognitoWindowTitleTextColor = SK_ColorWHITE;

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
                                 const cc::PaintFlags& flags,
                                 const gfx::Rect& bounds,
                                 int corner_radius,
                                 int image_inset_x) {
  SkPath frame_path = MakeRoundRectPath(bounds, corner_radius, corner_radius);
  // If |flags| is using an unusual SkBlendMode (this is the case while
  // crossfading), we must create a new canvas to overlay |frame_image| and
  // |frame_overlay_image| using |kSrcOver| and then paint the result
  // using the unusual mode. We try to avoid this because creating a new
  // browser-width canvas is expensive.
  bool fast_path = (frame_overlay_image.isNull() || flags.isSrcOver());
  if (fast_path) {
    if (frame_image.isNull()) {
      canvas->DrawPath(frame_path, flags);
    } else {
      canvas->DrawImageInPath(frame_image, -image_inset_x, 0, frame_path,
                              flags);
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
          flags);
    }
  } else {
    gfx::Canvas temporary_canvas(bounds.size(), canvas->image_scale(), false);
    if (frame_image.isNull()) {
      temporary_canvas.DrawColor(flags.getColor());
    } else {
      temporary_canvas.TileImageInt(frame_image, image_inset_x, 0, 0, 0,
                                    bounds.width(), bounds.height());
    }
    temporary_canvas.DrawImageInt(frame_overlay_image, 0, 0);
    canvas->DrawImageInPath(gfx::ImageSkia(temporary_canvas.ExtractImageRep()),
                            0, 0, frame_path, flags);
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
    BrowserNonClientFrameViewAsh* header_view,
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
  // Use light images in incognito, even when a custom theme is installed. The
  // incognito window with a custom theme is still darker than a normal window.
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

  PaintFrameImages(canvas, false);
  PaintFrameImages(canvas, true);

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

void BrowserHeaderPainterAsh::PaintFrameImages(gfx::Canvas* canvas,
                                               bool active) {
  int alpha = activation_animation_->CurrentValueBetween(0, 0xFF);
  if (!active)
    alpha = 0xFF - alpha;

  if (alpha == 0)
    return;

  bool round_corners = !frame_->IsMaximized() && !frame_->IsFullscreen();
  gfx::ImageSkia frame_image = view_->GetFrameImage(active);
  gfx::ImageSkia frame_overlay_image = view_->GetFrameOverlayImage(active);

  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kPlus);
  flags.setAlpha(alpha);
  flags.setColor(SkColorSetA(view_->GetFrameColor(active), alpha));
  flags.setAntiAlias(round_corners);
  PaintFrameImagesInRoundRect(
      canvas, frame_image, frame_overlay_image, flags, GetPaintedBounds(),
      round_corners ? ash::HeaderPainterUtil::GetTopCornerRadiusWhenRestored()
                    : 0,
      ash::HeaderPainterUtil::GetThemeBackgroundXInset());
}

void BrowserHeaderPainterAsh::PaintTitleBar(gfx::Canvas* canvas) {
  // The window icon is painted by its own views::View.
  gfx::Rect title_bounds = GetTitleBounds();
  title_bounds.set_x(view_->GetMirroredXForRect(title_bounds));
  canvas->DrawStringRectWithFlags(frame_->widget_delegate()->GetWindowTitle(),
                                  BrowserFrame::GetTitleFontList(),
                                  is_incognito_ ? kIncognitoWindowTitleTextColor
                                                : kNormalWindowTitleTextColor,
                                  title_bounds,
                                  gfx::Canvas::NO_SUBPIXEL_RENDERING);
}

void BrowserHeaderPainterAsh::UpdateCaptionButtons() {
  caption_button_container_->SetButtonImage(ash::CAPTION_BUTTON_ICON_MINIMIZE,
                                            ash::kWindowControlMinimizeIcon);
  caption_button_container_->SetButtonImage(ash::CAPTION_BUTTON_ICON_CLOSE,
                                            ash::kWindowControlCloseIcon);
  caption_button_container_->SetButtonImage(
      ash::CAPTION_BUTTON_ICON_LEFT_SNAPPED,
      ash::kWindowControlLeftSnappedIcon);
  caption_button_container_->SetButtonImage(
      ash::CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
      ash::kWindowControlRightSnappedIcon);

  const gfx::VectorIcon* size_icon = &ash::kWindowControlMaximizeIcon;
  gfx::Size button_size(
      GetAshLayoutSize(AshLayoutSize::BROWSER_RESTORED_CAPTION_BUTTON));
  if (frame_->IsMaximized() || frame_->IsFullscreen()) {
    size_icon = &ash::kWindowControlRestoreIcon;
    button_size =
        GetAshLayoutSize(AshLayoutSize::BROWSER_MAXIMIZED_CAPTION_BUTTON);
  }
  caption_button_container_->SetButtonImage(
      ash::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE, *size_icon);
  caption_button_container_->SetButtonSize(button_size);
}

gfx::Rect BrowserHeaderPainterAsh::GetPaintedBounds() const {
  return gfx::Rect(view_->width(), painted_height_);
}

gfx::Rect BrowserHeaderPainterAsh::GetTitleBounds() const {
  return ash::HeaderPainterUtil::GetTitleBounds(window_icon_,
      caption_button_container_, BrowserFrame::GetTitleFontList());
}
