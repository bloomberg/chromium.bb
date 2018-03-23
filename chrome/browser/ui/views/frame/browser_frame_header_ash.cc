// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_header_ash.h"

#include "ash/ash_layout_constants.h"
#include "ash/frame/caption_buttons/frame_caption_button.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/frame/frame_header_util.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "base/logging.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
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
#include "ui/gfx/scoped_canvas.h"
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
  SkScalar radii[8] = {kTopLeftRadius,
                       kTopLeftRadius,  // top-left
                       kTopRightRadius,
                       kTopRightRadius,  // top-right
                       0,
                       0,  // bottom-right
                       0,
                       0};  // bottom-left
  SkPath path;
  path.addRoundRect(rect, radii, SkPath::kCW_Direction);
  return path;
}

// Tiles |frame_image| and |frame_overlay_image| into an area, rounding the top
// corners.
void PaintFrameImagesInRoundRect(gfx::Canvas* canvas,
                                 const gfx::ImageSkia& frame_image,
                                 const gfx::ImageSkia& frame_overlay_image,
                                 int alpha,
                                 SkColor background_color,
                                 const gfx::Rect& bounds,
                                 int corner_radius,
                                 int image_inset_x) {
  SkPath frame_path = MakeRoundRectPath(bounds, corner_radius, corner_radius);
  bool antialias = corner_radius > 0;

  gfx::ScopedCanvas scoped_save(canvas);
  canvas->ClipPath(frame_path, antialias);

  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kPlus);
  flags.setAntiAlias(antialias);

  if (frame_image.isNull() && frame_overlay_image.isNull()) {
    flags.setColor(background_color);
    canvas->DrawRect(bounds, flags);
  } else if (frame_overlay_image.isNull()) {
    flags.setAlpha(alpha);
    canvas->TileImageInt(frame_image, image_inset_x, 0, 0, 0, bounds.width(),
                         bounds.height(), 1.0f, &flags);
  } else {
    flags.setAlpha(alpha);
    canvas->SaveLayerWithFlags(flags);

    if (frame_image.isNull()) {
      canvas->DrawColor(background_color);
    } else {
      canvas->TileImageInt(frame_image, image_inset_x, 0, 0, 0, bounds.width(),
                           bounds.height());
    }
    canvas->DrawImageInt(frame_overlay_image, 0, 0);

    canvas->Restore();
  }
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameHeaderAsh, public:

BrowserFrameHeaderAsh::BrowserFrameHeaderAsh()
    : frame_(nullptr),
      is_tabbed_(false),
      is_incognito_(false),
      view_(nullptr),
      window_icon_(nullptr),
      caption_button_container_(nullptr),
      painted_height_(0),
      initial_paint_(true),
      mode_(MODE_INACTIVE),
      activation_animation_(new gfx::SlideAnimation(this)) {}

BrowserFrameHeaderAsh::~BrowserFrameHeaderAsh() {}

void BrowserFrameHeaderAsh::Init(
    views::Widget* frame,
    BrowserView* browser_view,
    BrowserNonClientFrameViewAsh* header_view,
    views::View* window_icon,
    ash::FrameCaptionButtonContainerView* caption_button_container,
    ash::FrameCaptionButton* back_button) {
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

  back_button_ = back_button;
  if (back_button_)
    back_button_->set_use_light_images(is_incognito_);
}

int BrowserFrameHeaderAsh::GetMinimumHeaderWidth() const {
  // Ensure we have enough space for the window icon and buttons. We allow
  // the title string to collapse to zero width.
  return GetTitleBounds().x() +
         caption_button_container_->GetMinimumSize().width();
}

void BrowserFrameHeaderAsh::PaintHeader(gfx::Canvas* canvas, Mode mode) {
  Mode old_mode = mode_;
  mode_ = mode;

  if (mode_ != old_mode) {
    if (!initial_paint_ && ash::FrameHeaderUtil::CanAnimateActivation(frame_)) {
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

void BrowserFrameHeaderAsh::LayoutHeader() {
  // Purposefully set |painted_height_| to an invalid value. We cannot use
  // |painted_height_| because the computation of |painted_height_| may depend
  // on having laid out the window controls.
  painted_height_ = -1;

  UpdateCaptionButtons();
  caption_button_container_->Layout();

  const gfx::Size caption_button_container_size =
      caption_button_container_->GetPreferredSize();
  caption_button_container_->SetBounds(
      view_->width() - caption_button_container_size.width(), 0,
      caption_button_container_size.width(),
      caption_button_container_size.height());

  if (window_icon_) {
    // Vertically center the window icon with respect to the caption button
    // container.
    const gfx::Size icon_size(window_icon_->GetPreferredSize());
    const int icon_offset_y = (GetHeaderHeight() - icon_size.height()) / 2;
    window_icon_->SetBounds(ash::FrameHeaderUtil::GetLeftViewXInset(),
                            icon_offset_y, icon_size.width(),
                            icon_size.height());
  }
}

int BrowserFrameHeaderAsh::GetHeaderHeight() const {
  return caption_button_container_->height();
}

int BrowserFrameHeaderAsh::GetHeaderHeightForPainting() const {
  return painted_height_;
}

void BrowserFrameHeaderAsh::SetHeaderHeightForPainting(int height) {
  painted_height_ = height;
}

void BrowserFrameHeaderAsh::SchedulePaintForTitle() {
  view_->SchedulePaintInRect(GetTitleBounds());
}

void BrowserFrameHeaderAsh::SetPaintAsActive(bool paint_as_active) {
  caption_button_container_->SetPaintAsActive(paint_as_active);
  if (back_button_)
    back_button_->set_paint_as_active(paint_as_active);
}

///////////////////////////////////////////////////////////////////////////////
// gfx::AnimationDelegate overrides:

void BrowserFrameHeaderAsh::AnimationProgressed(
    const gfx::Animation* animation) {
  view_->SchedulePaintInRect(GetPaintedBounds());
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameHeaderAsh, private:

void BrowserFrameHeaderAsh::PaintFrameImages(gfx::Canvas* canvas, bool active) {
  int alpha = activation_animation_->CurrentValueBetween(0, 0xFF);
  if (!active)
    alpha = 0xFF - alpha;

  if (alpha == 0)
    return;

  gfx::ImageSkia frame_image = view_->GetFrameImage(active);
  gfx::ImageSkia frame_overlay_image = view_->GetFrameOverlayImage(active);
  SkColor background_color = SkColorSetA(view_->GetFrameColor(active), alpha);

  int corner_radius = 0;
  if (!frame_->IsMaximized() && !frame_->IsFullscreen())
    corner_radius = ash::FrameHeaderUtil::GetTopCornerRadiusWhenRestored();

  PaintFrameImagesInRoundRect(canvas, frame_image, frame_overlay_image, alpha,
                              background_color, GetPaintedBounds(),
                              corner_radius,
                              ash::FrameHeaderUtil::GetThemeBackgroundXInset());
}

void BrowserFrameHeaderAsh::PaintTitleBar(gfx::Canvas* canvas) {
  // The window icon is painted by its own views::View.
  canvas->DrawStringRectWithFlags(frame_->widget_delegate()->GetWindowTitle(),
                                  BrowserFrame::GetTitleFontList(),
                                  is_incognito_ ? kIncognitoWindowTitleTextColor
                                                : kNormalWindowTitleTextColor,
                                  view_->GetMirroredRect(GetTitleBounds()),
                                  gfx::Canvas::NO_SUBPIXEL_RENDERING);
}

void BrowserFrameHeaderAsh::UpdateCaptionButtons() {
  // When |frame_| minimized, avoid tablet mode toggling to update caption
  // buttons as it would cause mismatch beteen window state and size button.
  if (frame_->IsMinimized())
    return;
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

  const bool is_in_tablet_mode =
      TabletModeClient::Get() && TabletModeClient::Get()->tablet_mode_enabled();
  const bool is_maximized_or_fullscreen =
      frame_->IsMaximized() || frame_->IsFullscreen();
  const AshLayoutSize button_size_type =
      is_maximized_or_fullscreen || is_in_tablet_mode
          ? AshLayoutSize::kBrowserCaptionMaximized
          : AshLayoutSize::kBrowserCaptionRestored;
  const gfx::VectorIcon* const size_icon =
      is_maximized_or_fullscreen ? &ash::kWindowControlRestoreIcon
                                 : &ash::kWindowControlMaximizeIcon;

  caption_button_container_->SetButtonImage(
      ash::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE, *size_icon);
  caption_button_container_->SetButtonSize(GetAshLayoutSize(button_size_type));
}

gfx::Rect BrowserFrameHeaderAsh::GetPaintedBounds() const {
  return gfx::Rect(view_->width(), painted_height_);
}

gfx::Rect BrowserFrameHeaderAsh::GetTitleBounds() const {
  views::View* left_view = window_icon_ ? window_icon_ : back_button_;
  return ash::FrameHeaderUtil::GetAvailableTitleBounds(
      left_view, caption_button_container_, BrowserFrame::GetTitleFontList(),
      GetHeaderHeight());
}
