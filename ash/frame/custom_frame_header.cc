// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/custom_frame_header.h"

#include "ash/ash_layout_constants.h"
#include "ash/frame/caption_buttons/frame_caption_button.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/frame/frame_header_util.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

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
// CustomFrameHeader, public:

CustomFrameHeader::CustomFrameHeader() = default;

CustomFrameHeader::~CustomFrameHeader() = default;

void CustomFrameHeader::Init(
    views::View* view,
    AppearanceProvider* appearance_provider,
    bool incognito,
    FrameCaptionButtonContainerView* caption_button_container) {
  DCHECK(view);
  DCHECK(appearance_provider);
  DCHECK(caption_button_container);

  view_ = view;
  appearance_provider_ = appearance_provider;
  is_incognito_ = incognito;
  caption_button_container_ = caption_button_container;
}

int CustomFrameHeader::GetMinimumHeaderWidth() const {
  // Ensure we have enough space for the window icon and buttons. We allow
  // the title string to collapse to zero width.
  return GetTitleBounds().x() +
         caption_button_container_->GetMinimumSize().width();
}

void CustomFrameHeader::PaintHeader(gfx::Canvas* canvas, Mode mode) {
  Mode old_mode = mode_;
  mode_ = mode;

  if (mode_ != old_mode) {
    if (!initial_paint_ && FrameHeaderUtil::CanAnimateActivation(GetWidget())) {
      activation_animation_.SetSlideDuration(kActivationCrossfadeDurationMs);
      if (mode_ == MODE_ACTIVE)
        activation_animation_.Show();
      else
        activation_animation_.Hide();
    } else {
      if (mode_ == MODE_ACTIVE)
        activation_animation_.Reset(1);
      else
        activation_animation_.Reset(0);
    }
    initial_paint_ = false;
  }

  PaintFrameImages(canvas, false);
  PaintFrameImages(canvas, true);

  if (GetWidget()->widget_delegate() &&
      GetWidget()->widget_delegate()->ShouldShowWindowTitle()) {
    PaintTitleBar(canvas);
  }
}

void CustomFrameHeader::LayoutHeader() {
  LayoutHeaderInternal();
  // Default to the header height; owning code may override via
  // SetHeaderHeightForPainting().
  painted_height_ = GetHeaderHeight();
}

int CustomFrameHeader::GetHeaderHeight() const {
  return caption_button_container_->height();
}

int CustomFrameHeader::GetHeaderHeightForPainting() const {
  return painted_height_;
}

void CustomFrameHeader::SetHeaderHeightForPainting(int height) {
  painted_height_ = height;
}

void CustomFrameHeader::SchedulePaintForTitle() {
  view_->SchedulePaintInRect(GetTitleBounds());
}

void CustomFrameHeader::SetPaintAsActive(bool paint_as_active) {
  SkColor frame_color =
      appearance_provider_->GetFrameHeaderColor(paint_as_active);
  caption_button_container_->SetPaintAsActive(paint_as_active);
  caption_button_container_->SetBackgroundColor(frame_color);
  if (back_button_) {
    back_button_->set_paint_as_active(paint_as_active);
    back_button_->SetBackgroundColor(frame_color);
  }
}

void CustomFrameHeader::OnShowStateChanged(ui::WindowShowState show_state) {
  if (show_state == ui::SHOW_STATE_MINIMIZED)
    return;
  // Call LayoutHeaderInternal() instead of LayoutHeader() here because
  // |show_state| shouldn't cause |painted_height_| change.
  LayoutHeaderInternal();
}

void CustomFrameHeader::SetLeftHeaderView(views::View* left_header_view) {
  window_icon_ = left_header_view;
}

void CustomFrameHeader::SetBackButton(FrameCaptionButton* back_button) {
  back_button_ = back_button;
}

FrameCaptionButton* CustomFrameHeader::GetBackButton() const {
  return back_button_;
}

void CustomFrameHeader::SetFrameColors(SkColor active_frame_color,
                                       SkColor inactive_frame_color) {
  view_->SchedulePaintInRect(GetPaintedBounds());
}

///////////////////////////////////////////////////////////////////////////////
// gfx::AnimationDelegate overrides:

void CustomFrameHeader::AnimationProgressed(const gfx::Animation* animation) {
  view_->SchedulePaintInRect(GetPaintedBounds());
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameHeader, private:

void CustomFrameHeader::LayoutHeaderInternal() {
  UpdateCaptionButtons();
  const gfx::Size caption_button_container_size =
      caption_button_container_->GetPreferredSize();
  caption_button_container_->SetBounds(
      view_->width() - caption_button_container_size.width(), 0,
      caption_button_container_size.width(),
      caption_button_container_size.height());

  caption_button_container_->Layout();

  if (!window_icon_)
    return;

  // Vertically center the window icon with respect to the caption button
  // container.
  const gfx::Size icon_size(window_icon_->GetPreferredSize());
  const int icon_offset_y = (GetHeaderHeight() - icon_size.height()) / 2;
  window_icon_->SetBounds(FrameHeaderUtil::GetLeftViewXInset(), icon_offset_y,
                          icon_size.width(), icon_size.height());
}

void CustomFrameHeader::PaintFrameImages(gfx::Canvas* canvas, bool active) {
  int alpha = activation_animation_.CurrentValueBetween(0, 0xFF);
  if (!active)
    alpha = 0xFF - alpha;

  if (alpha == 0)
    return;

  gfx::ImageSkia frame_image =
      appearance_provider_->GetFrameHeaderImage(active);
  gfx::ImageSkia frame_overlay_image =
      appearance_provider_->GetFrameHeaderOverlayImage(active);
  SkColor background_color =
      SkColorSetA(appearance_provider_->GetFrameHeaderColor(active), alpha);

  int corner_radius = 0;
  if (!GetWidget()->IsMaximized() && !GetWidget()->IsFullscreen())
    corner_radius = FrameHeaderUtil::GetTopCornerRadiusWhenRestored();

  PaintFrameImagesInRoundRect(canvas, frame_image, frame_overlay_image, alpha,
                              background_color, GetPaintedBounds(),
                              corner_radius,
                              FrameHeaderUtil::GetThemeBackgroundXInset());
}

void CustomFrameHeader::PaintTitleBar(gfx::Canvas* canvas) {
  // The window icon is painted by its own views::View.
  canvas->DrawStringRectWithFlags(
      GetWidget()->widget_delegate()->GetWindowTitle(),
      views::NativeWidgetAura::GetWindowTitleFontList(),
      is_incognito_ ? kIncognitoWindowTitleTextColor
                    : kNormalWindowTitleTextColor,
      view_->GetMirroredRect(GetTitleBounds()),
      gfx::Canvas::NO_SUBPIXEL_RENDERING);
}

void CustomFrameHeader::UpdateCaptionButtons() {
  // When |frame_| minimized, avoid tablet mode toggling to update caption
  // buttons as it would cause mismatch beteen window state and size button.
  if (GetWidget()->IsMinimized())
    return;
  caption_button_container_->SetButtonImage(CAPTION_BUTTON_ICON_MINIMIZE,
                                            kWindowControlMinimizeIcon);
  caption_button_container_->SetButtonImage(CAPTION_BUTTON_ICON_CLOSE,
                                            kWindowControlCloseIcon);
  caption_button_container_->SetButtonImage(CAPTION_BUTTON_ICON_LEFT_SNAPPED,
                                            kWindowControlLeftSnappedIcon);
  caption_button_container_->SetButtonImage(CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
                                            kWindowControlRightSnappedIcon);

  const bool is_maximized_or_fullscreen =
      GetWidget()->IsMaximized() || GetWidget()->IsFullscreen();
  const AshLayoutSize button_size_type =
      is_maximized_or_fullscreen || appearance_provider_->IsTabletMode()
          ? AshLayoutSize::kBrowserCaptionMaximized
          : AshLayoutSize::kBrowserCaptionRestored;
  const gfx::VectorIcon* const size_icon = is_maximized_or_fullscreen
                                               ? &kWindowControlRestoreIcon
                                               : &kWindowControlMaximizeIcon;

  caption_button_container_->SetButtonImage(
      CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE, *size_icon);
  caption_button_container_->SetButtonSize(GetAshLayoutSize(button_size_type));
}

gfx::Rect CustomFrameHeader::GetPaintedBounds() const {
  return gfx::Rect(view_->width(), painted_height_);
}

gfx::Rect CustomFrameHeader::GetTitleBounds() const {
  views::View* left_view = window_icon_ ? window_icon_ : back_button_;
  return FrameHeaderUtil::GetAvailableTitleBounds(
      left_view, caption_button_container_, GetHeaderHeight());
}

views::Widget* CustomFrameHeader::GetWidget() {
  return view_->GetWidget();
}

}  // namespace ash
