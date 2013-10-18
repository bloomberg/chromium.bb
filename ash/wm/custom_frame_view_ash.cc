// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/custom_frame_view_ash.h"

#include "ash/wm/caption_buttons/frame_caption_button_container_view.h"
#include "ash/wm/frame_border_hit_test_controller.h"
#include "ash/wm/header_painter.h"
#include "grit/ash_resources.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

const gfx::Font& GetTitleFont() {
  static gfx::Font* title_font = NULL;
  if (!title_font)
    title_font = new gfx::Font(views::NativeWidgetAura::GetWindowTitleFont());
  return *title_font;
}

}  // namespace

namespace ash {

// static
const char CustomFrameViewAsh::kViewClassName[] = "CustomFrameViewAsh";

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, public:
CustomFrameViewAsh::CustomFrameViewAsh(views::Widget* frame)
    : frame_(frame),
      caption_button_container_(NULL),
      header_painter_(new ash::HeaderPainter),
      frame_border_hit_test_controller_(
          new FrameBorderHitTestController(frame_)) {
  // Unfortunately, there is no views::WidgetDelegate::CanMinimize(). Assume
  // that the window frame can be minimized if it can be maximized.
  FrameCaptionButtonContainerView::MinimizeAllowed minimize_allowed =
      frame_->widget_delegate()->CanMaximize() ?
          FrameCaptionButtonContainerView::MINIMIZE_ALLOWED :
          FrameCaptionButtonContainerView::MINIMIZE_DISALLOWED;
  caption_button_container_ = new FrameCaptionButtonContainerView(frame,
      minimize_allowed);
  AddChildView(caption_button_container_);

  header_painter_->Init(frame_, this, NULL, caption_button_container_);
}

CustomFrameViewAsh::~CustomFrameViewAsh() {
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, views::NonClientFrameView overrides:
gfx::Rect CustomFrameViewAsh::GetBoundsForClientView() const {
  int top_height = NonClientTopBorderHeight();
  return HeaderPainter::GetBoundsForClientView(top_height, bounds());
}

gfx::Rect CustomFrameViewAsh::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight();
  return HeaderPainter::GetWindowBoundsForClientBounds(top_height,
                                                       client_bounds);
}

int CustomFrameViewAsh::NonClientHitTest(const gfx::Point& point) {
  return FrameBorderHitTestController::NonClientHitTest(this,
      header_painter_.get(), point);
}

void CustomFrameViewAsh::GetWindowMask(const gfx::Size& size,
                                       gfx::Path* window_mask) {
  // No window masks in Aura.
}

void CustomFrameViewAsh::ResetWindowControls() {
  caption_button_container_->ResetWindowControls();
}

void CustomFrameViewAsh::UpdateWindowIcon() {
}

void CustomFrameViewAsh::UpdateWindowTitle() {
  header_painter_->SchedulePaintForTitle(GetTitleFont());
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, views::View overrides:

gfx::Size CustomFrameViewAsh::GetPreferredSize() {
  gfx::Size pref = frame_->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return frame_->non_client_view()->GetWindowBoundsForClientBounds(
      bounds).size();
}

void CustomFrameViewAsh::Layout() {
  // Use the shorter maximized layout headers.
  header_painter_->LayoutHeader(true);
  header_painter_->set_header_height(NonClientTopBorderHeight());
}

void CustomFrameViewAsh::OnPaint(gfx::Canvas* canvas) {
  if (frame_->IsFullscreen())
    return;

  // Prevent bleeding paint onto the client area below the window frame, which
  // may become visible when the WebContent is transparent.
  canvas->Save();
  canvas->ClipRect(gfx::Rect(0, 0, width(), NonClientTopBorderHeight()));

  bool paint_as_active = ShouldPaintAsActive();
  int theme_image_id = 0;
  if (header_painter_->ShouldUseMinimalHeaderStyle(HeaderPainter::THEMED_NO))
    theme_image_id = IDR_AURA_WINDOW_HEADER_BASE_MINIMAL;
  else if (paint_as_active)
    theme_image_id = IDR_AURA_WINDOW_HEADER_BASE_ACTIVE;
  else
    theme_image_id = IDR_AURA_WINDOW_HEADER_BASE_INACTIVE;

  header_painter_->PaintHeader(
      canvas,
      paint_as_active ? HeaderPainter::ACTIVE : HeaderPainter::INACTIVE,
      theme_image_id,
      0);
  header_painter_->PaintTitleBar(canvas, GetTitleFont());
  header_painter_->PaintHeaderContentSeparator(canvas);
  canvas->Restore();
}

const char* CustomFrameViewAsh::GetClassName() const {
  return kViewClassName;
}

gfx::Size CustomFrameViewAsh::GetMinimumSize() {
  gfx::Size min_client_view_size(frame_->client_view()->GetMinimumSize());
  return gfx::Size(
      std::max(header_painter_->GetMinimumHeaderWidth(),
               min_client_view_size.width()),
      NonClientTopBorderHeight() + min_client_view_size.height());
}

gfx::Size CustomFrameViewAsh::GetMaximumSize() {
  return frame_->client_view()->GetMaximumSize();
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, private:

int CustomFrameViewAsh::NonClientTopBorderHeight() const {
  if (frame_->IsFullscreen())
    return 0;

  // Reserve enough space to see the buttons, including any offset from top and
  // reserving space for the separator line.
  return caption_button_container_->bounds().bottom() +
      header_painter_->HeaderContentSeparatorSize();
}

}  // namespace ash
