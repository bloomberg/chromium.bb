// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/custom_frame_view_ash.h"

#include "ash/wm/frame_painter.h"
#include "ash/wm/workspace/frame_caption_button_container_view.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
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
CustomFrameViewAsh::CustomFrameViewAsh()
    : frame_(NULL),
      caption_button_container_(NULL),
      frame_painter_(new ash::FramePainter) {
}

CustomFrameViewAsh::~CustomFrameViewAsh() {
}

void CustomFrameViewAsh::Init(views::Widget* frame) {
  frame_ = frame;

  // Unfortunately, there is no views::WidgetDelegate::CanMinimize(). Assume
  // that the window frame can be minimized if it can be maximized.
  FrameCaptionButtonContainerView::MinimizeAllowed minimize_allowed =
      frame_->widget_delegate()->CanMaximize() ?
          FrameCaptionButtonContainerView::MINIMIZE_ALLOWED :
          FrameCaptionButtonContainerView::MINIMIZE_DISALLOWED;
  caption_button_container_ = new FrameCaptionButtonContainerView(this, frame,
      minimize_allowed);
  AddChildView(caption_button_container_);

  frame_painter_->Init(frame_, NULL, caption_button_container_);
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, views::NonClientFrameView overrides:
gfx::Rect CustomFrameViewAsh::GetBoundsForClientView() const {
  int top_height = NonClientTopBorderHeight();
  return frame_painter_->GetBoundsForClientView(top_height,
                                                bounds());
}

gfx::Rect CustomFrameViewAsh::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight();
  return frame_painter_->GetWindowBoundsForClientBounds(top_height,
                                                        client_bounds);
}

int CustomFrameViewAsh::NonClientHitTest(const gfx::Point& point) {
  return frame_painter_->NonClientHitTest(this, point);
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
  frame_painter_->SchedulePaintForTitle(GetTitleFont());
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
  frame_painter_->LayoutHeader(this, true);
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
  if (frame_painter_->ShouldUseMinimalHeaderStyle(FramePainter::THEMED_NO))
    theme_image_id = IDR_AURA_WINDOW_HEADER_BASE_MINIMAL;
  else if (paint_as_active)
    theme_image_id = IDR_AURA_WINDOW_HEADER_BASE_ACTIVE;
  else
    theme_image_id = IDR_AURA_WINDOW_HEADER_BASE_INACTIVE;

  frame_painter_->PaintHeader(
      this,
      canvas,
      paint_as_active ? FramePainter::ACTIVE : FramePainter::INACTIVE,
      theme_image_id,
      0);
  frame_painter_->PaintTitleBar(this, canvas, GetTitleFont());
  frame_painter_->PaintHeaderContentSeparator(this, canvas);
  canvas->Restore();
}

const char* CustomFrameViewAsh::GetClassName() const {
  return kViewClassName;
}

gfx::Size CustomFrameViewAsh::GetMinimumSize() {
  return frame_painter_->GetMinimumSize(this);
}

gfx::Size CustomFrameViewAsh::GetMaximumSize() {
  return frame_painter_->GetMaximumSize(this);
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, private:

int CustomFrameViewAsh::NonClientTopBorderHeight() const {
  if (frame_->IsFullscreen())
    return 0;

  // Reserve enough space to see the buttons, including any offset from top and
  // reserving space for the separator line.
  return caption_button_container_->bounds().bottom() +
      frame_painter_->HeaderContentSeparatorSize();
}

}  // namespace ash
