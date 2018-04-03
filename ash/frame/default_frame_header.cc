// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/default_frame_header.h"

#include "ash/ash_layout_constants.h"
#include "ash/frame/caption_buttons/caption_button_model.h"
#include "ash/frame/caption_buttons/frame_caption_button.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/frame/frame_header_util.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/debug/leak_annotations.h"
#include "base/logging.h"  // DCHECK
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using views::Widget;

namespace {

// Color for the window title text.
const SkColor kTitleTextColor = SkColorSetRGB(40, 40, 40);
const SkColor kLightTitleTextColor = SK_ColorWHITE;
// Color of the active window header/content separator line.
const SkColor kHeaderContentSeparatorColor = SkColorSetRGB(150, 150, 152);
// Color of the inactive window header/content separator line.
const SkColor kHeaderContentSeparatorInactiveColor =
    SkColorSetRGB(180, 180, 182);
// The default color of the frame.
const SkColor kDefaultFrameColor = SkColorSetRGB(242, 242, 242);
// Duration of crossfade animation for activating and deactivating frame.
const int kActivationCrossfadeDurationMs = 200;

// Tiles an image into an area, rounding the top corners.
void TileRoundRect(gfx::Canvas* canvas,
                   const cc::PaintFlags& flags,
                   const gfx::Rect& bounds,
                   int corner_radius) {
  SkRect rect = gfx::RectToSkRect(bounds);
  const SkScalar corner_radius_scalar = SkIntToScalar(corner_radius);
  SkScalar radii[8] = {corner_radius_scalar,
                       corner_radius_scalar,  // top-left
                       corner_radius_scalar,
                       corner_radius_scalar,  // top-right
                       0,
                       0,  // bottom-right
                       0,
                       0};  // bottom-left
  SkPath path;
  path.addRoundRect(rect, radii, SkPath::kCW_Direction);
  canvas->DrawPath(path, flags);
}

}  // namespace

namespace ash {

///////////////////////////////////////////////////////////////////////////////
// DefaultFrameHeader, public:

DefaultFrameHeader::DefaultFrameHeader(
    views::Widget* frame,
    views::View* header_view,
    FrameCaptionButtonContainerView* caption_button_container,
    mojom::WindowStyle window_style)
    : window_style_(window_style),
      frame_(frame),
      view_(header_view),
      back_button_(nullptr),
      left_header_view_(nullptr),
      active_frame_color_(kDefaultFrameColor),
      inactive_frame_color_(kDefaultFrameColor),
      caption_button_container_(caption_button_container),
      painted_height_(0),
      mode_(MODE_INACTIVE),
      initial_paint_(true),
      activation_animation_(new gfx::SlideAnimation(this)) {
  DCHECK(frame);
  DCHECK(header_view);
  DCHECK(caption_button_container);
  caption_button_container_->SetButtonSize(
      GetAshLayoutSize(AshLayoutSize::kNonBrowserCaption));
  UpdateAllButtonImages();
}

DefaultFrameHeader::~DefaultFrameHeader() = default;

int DefaultFrameHeader::GetMinimumHeaderWidth() const {
  // Ensure we have enough space for the window icon and buttons. We allow
  // the title string to collapse to zero width.
  return GetAvailableTitleBounds().x() +
         caption_button_container_->GetMinimumSize().width();
}

void DefaultFrameHeader::PaintHeader(gfx::Canvas* canvas, Mode mode) {
  Mode old_mode = mode_;
  mode_ = mode;

  if (mode_ != old_mode) {
    UpdateAllButtonImages();
    if (!initial_paint_ && FrameHeaderUtil::CanAnimateActivation(frame_)) {
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

  int corner_radius = (frame_->IsMaximized() || frame_->IsFullscreen())
                          ? 0
                          : FrameHeaderUtil::GetTopCornerRadiusWhenRestored();

  cc::PaintFlags flags;
  int active_alpha = activation_animation_->CurrentValueBetween(0, 255);
  flags.setColor(color_utils::AlphaBlend(active_frame_color_,
                                         inactive_frame_color_, active_alpha));
  flags.setAntiAlias(true);
  TileRoundRect(canvas, flags, GetLocalBounds(), corner_radius);

  if (!frame_->IsMaximized() && !frame_->IsFullscreen() &&
      mode_ == MODE_INACTIVE && !UsesCustomFrameColors()) {
    PaintHighlightForInactiveRestoredWindow(canvas);
  }
  if (frame_->widget_delegate()->ShouldShowWindowTitle())
    PaintTitleBar(canvas);
  if (!UsesCustomFrameColors())
    PaintHeaderContentSeparator(canvas);
}

void DefaultFrameHeader::LayoutHeader() {
  // TODO(sky): this needs to reset images as well.
  if (window_style_ == mojom::WindowStyle::BROWSER) {
    const bool is_in_tablet_mode = Shell::Get()
                                       ->tablet_mode_controller()
                                       ->IsTabletModeWindowManagerEnabled();
    const bool use_maximized_size =
        frame_->IsMaximized() || frame_->IsFullscreen() || is_in_tablet_mode;
    const gfx::Size button_size(GetAshLayoutSize(
        use_maximized_size ? AshLayoutSize::kBrowserCaptionMaximized
                           : AshLayoutSize::kBrowserCaptionRestored));
    caption_button_container_->SetButtonSize(button_size);
  }

  caption_button_container_->SetUseLightImages(ShouldUseLightImages());
  UpdateSizeButtonImages();

  gfx::Size caption_button_container_size =
      caption_button_container_->GetPreferredSize();
  caption_button_container_->SetBounds(
      view_->width() - caption_button_container_size.width(), 0,
      caption_button_container_size.width(),
      caption_button_container_size.height());

  int origin = 0;
  if (back_button_) {
    back_button_->set_use_light_images(ShouldUseLightImages());
    gfx::Size size = back_button_->GetPreferredSize();
    back_button_->SetBounds(0, 0, size.width(),
                            caption_button_container_size.height());
    origin = back_button_->bounds().right();
  }

  if (left_header_view_) {
    // Vertically center the left header view with respect to the caption button
    // container.
    // Floor when computing the center of |caption_button_container_|.
    gfx::Size size = left_header_view_->GetPreferredSize();
    int icon_offset_y =
        caption_button_container_->height() / 2 - size.height() / 2;
    left_header_view_->SetBounds(FrameHeaderUtil::GetLeftViewXInset() + origin,
                                 icon_offset_y, size.width(), size.height());
  }

  // The header/content separator line overlays the caption buttons.
  SetHeaderHeightForPainting(caption_button_container_->height());
}

int DefaultFrameHeader::GetHeaderHeight() const {
  return caption_button_container_->height();
}

int DefaultFrameHeader::GetHeaderHeightForPainting() const {
  return painted_height_;
}

void DefaultFrameHeader::SetHeaderHeightForPainting(int height) {
  painted_height_ = height;
}

void DefaultFrameHeader::SchedulePaintForTitle() {
  view_->SchedulePaintInRect(GetAvailableTitleBounds());
}

void DefaultFrameHeader::SetPaintAsActive(bool paint_as_active) {
  caption_button_container_->SetPaintAsActive(paint_as_active);
  if (back_button_)
    back_button_->set_paint_as_active(paint_as_active);
}

void DefaultFrameHeader::SetFrameColors(SkColor active_frame_color,
                                        SkColor inactive_frame_color) {
  bool updated = false;
  if (active_frame_color_ != active_frame_color) {
    active_frame_color_ = active_frame_color;
    updated = true;
  }
  if (inactive_frame_color_ != inactive_frame_color) {
    inactive_frame_color_ = inactive_frame_color;
    updated = true;
  }

  if (updated)
    UpdateAllButtonImages();
}

SkColor DefaultFrameHeader::GetActiveFrameColor() const {
  return active_frame_color_;
}

SkColor DefaultFrameHeader::GetInactiveFrameColor() const {
  return inactive_frame_color_;
}

SkColor DefaultFrameHeader::GetTitleColor() const {
  return ShouldUseLightImages() ? kLightTitleTextColor : kTitleTextColor;
}

bool DefaultFrameHeader::ShouldUseLightImages() const {
  return color_utils::IsDark(mode_ == MODE_INACTIVE ? inactive_frame_color_
                                                    : active_frame_color_);
}

///////////////////////////////////////////////////////////////////////////////
// gfx::AnimationDelegate overrides:

void DefaultFrameHeader::AnimationProgressed(const gfx::Animation* animation) {
  view_->SchedulePaintInRect(GetLocalBounds());
}

///////////////////////////////////////////////////////////////////////////////
// DefaultFrameHeader, private:

void DefaultFrameHeader::PaintHighlightForInactiveRestoredWindow(
    gfx::Canvas* canvas) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia top_edge =
      *rb.GetImageSkiaNamed(IDR_AURA_WINDOW_HEADER_SHADE_INACTIVE_TOP);
  gfx::ImageSkia left_edge =
      *rb.GetImageSkiaNamed(IDR_AURA_WINDOW_HEADER_SHADE_INACTIVE_LEFT);
  gfx::ImageSkia right_edge =
      *rb.GetImageSkiaNamed(IDR_AURA_WINDOW_HEADER_SHADE_INACTIVE_RIGHT);
  gfx::ImageSkia bottom_edge =
      *rb.GetImageSkiaNamed(IDR_AURA_WINDOW_HEADER_SHADE_INACTIVE_BOTTOM);

  int left_edge_width = left_edge.width();
  int right_edge_width = right_edge.width();
  canvas->DrawImageInt(left_edge, 0, 0);
  canvas->DrawImageInt(right_edge, view_->width() - right_edge_width, 0);
  canvas->TileImageInt(top_edge, left_edge_width, 0,
                       view_->width() - left_edge_width - right_edge_width,
                       top_edge.height());

  DCHECK_EQ(left_edge.height(), right_edge.height());
  int bottom = left_edge.height();
  int bottom_height = bottom_edge.height();
  canvas->TileImageInt(bottom_edge, left_edge_width, bottom - bottom_height,
                       view_->width() - left_edge_width - right_edge_width,
                       bottom_height);
}

void DefaultFrameHeader::PaintTitleBar(gfx::Canvas* canvas) {
  // The window icon is painted by its own views::View.
  gfx::Rect title_bounds = GetAvailableTitleBounds();
  title_bounds.set_x(view_->GetMirroredXForRect(title_bounds));
  canvas->DrawStringRect(frame_->widget_delegate()->GetWindowTitle(),
                         views::NativeWidgetAura::GetWindowTitleFontList(),
                         GetTitleColor(), title_bounds);
}

void DefaultFrameHeader::PaintHeaderContentSeparator(gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();
  gfx::RectF rect(0, painted_height_ * scale - 1, view_->width() * scale, 1);
  cc::PaintFlags flags;
  flags.setColor((mode_ == MODE_ACTIVE) ? kHeaderContentSeparatorColor
                                        : kHeaderContentSeparatorInactiveColor);
  canvas->sk_canvas()->drawRect(gfx::RectFToSkRect(rect), flags);
}

void DefaultFrameHeader::UpdateAllButtonImages() {
  caption_button_container_->SetUseLightImages(ShouldUseLightImages());
  if (back_button_) {
    back_button_->set_use_light_images(ShouldUseLightImages());
    back_button_->SetImage(CAPTION_BUTTON_ICON_BACK,
                           FrameCaptionButton::ANIMATE_NO,
                           kWindowControlBackIcon);
  }

  caption_button_container_->SetButtonImage(CAPTION_BUTTON_ICON_MINIMIZE,
                                            kWindowControlMinimizeIcon);

  UpdateSizeButtonImages();

  caption_button_container_->SetButtonImage(CAPTION_BUTTON_ICON_MENU,
                                            kWindowControlMenuIcon);

  caption_button_container_->SetButtonImage(CAPTION_BUTTON_ICON_CLOSE,
                                            kWindowControlCloseIcon);

  caption_button_container_->SetButtonImage(CAPTION_BUTTON_ICON_LEFT_SNAPPED,
                                            kWindowControlLeftSnappedIcon);

  caption_button_container_->SetButtonImage(CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
                                            kWindowControlRightSnappedIcon);
}

void DefaultFrameHeader::UpdateSizeButtonImages() {
  // When |frame_| minimized, avoid tablet mode toggling to update caption
  // buttons as it would cause mismatch beteen window state and size button.
  if (frame_->IsMinimized())
    return;
  bool use_zoom_icons = caption_button_container_->model()->InZoomMode();
  const gfx::VectorIcon& restore_icon =
      use_zoom_icons ? kWindowControlDezoomIcon : kWindowControlRestoreIcon;
  const gfx::VectorIcon& maximize_icon =
      use_zoom_icons ? kWindowControlZoomIcon : kWindowControlMaximizeIcon;
  const gfx::VectorIcon& icon = frame_->IsMaximized() || frame_->IsFullscreen()
                                    ? restore_icon
                                    : maximize_icon;
  caption_button_container_->SetButtonImage(
      CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE, icon);
}

gfx::Rect DefaultFrameHeader::GetLocalBounds() const {
  return gfx::Rect(view_->width(), painted_height_);
}

gfx::Rect DefaultFrameHeader::GetAvailableTitleBounds() const {
  views::View* left_view = left_header_view_ ? left_header_view_ : back_button_;
  return FrameHeaderUtil::GetAvailableTitleBounds(
      left_view, caption_button_container_,
      views::NativeWidgetAura::GetWindowTitleFontList(), GetHeaderHeight());
}

bool DefaultFrameHeader::UsesCustomFrameColors() const {
  return active_frame_color_ != kDefaultFrameColor ||
         inactive_frame_color_ != kDefaultFrameColor;
}

}  // namespace ash
