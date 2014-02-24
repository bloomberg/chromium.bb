// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panels/panel_frame_view.h"

#include "ash/wm/caption_buttons/frame_caption_button_container_view.h"
#include "ash/wm/frame_border_hit_test_controller.h"
#include "ash/wm/header_painter.h"
#include "grit/ash_resources.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

// static
const char PanelFrameView::kViewClassName[] = "PanelFrameView";

PanelFrameView::PanelFrameView(views::Widget* frame, FrameType frame_type)
    : frame_(frame),
      caption_button_container_(NULL),
      window_icon_(NULL),
      title_font_list_(views::NativeWidgetAura::GetWindowTitleFontList()),
      frame_border_hit_test_controller_(
          new FrameBorderHitTestController(frame_)) {
  DCHECK(!frame_->widget_delegate()->CanMaximize());
  if (frame_type != FRAME_NONE)
    InitHeaderPainter();
}

PanelFrameView::~PanelFrameView() {
}

const char* PanelFrameView::GetClassName() const {
  return kViewClassName;
}

void PanelFrameView::InitHeaderPainter() {
  header_painter_.reset(new HeaderPainter);

  caption_button_container_ = new FrameCaptionButtonContainerView(frame_,
      FrameCaptionButtonContainerView::MINIMIZE_ALLOWED);
  AddChildView(caption_button_container_);

  if (frame_->widget_delegate()->ShouldShowWindowIcon()) {
    window_icon_ = new views::ImageView();
    AddChildView(window_icon_);
  }

  header_painter_->Init(HeaderPainter::STYLE_OTHER, frame_, this, window_icon_,
      caption_button_container_);
}

int PanelFrameView::NonClientTopBorderHeight() const {
  if (!header_painter_)
    return 0;
  // Reserve enough space to see the buttons and the separator line.
  return caption_button_container_->bounds().bottom() +
      header_painter_->HeaderContentSeparatorSize();
}

gfx::Size PanelFrameView::GetMinimumSize() {
  if (!header_painter_)
    return gfx::Size();
  gfx::Size min_client_view_size(frame_->client_view()->GetMinimumSize());
  return gfx::Size(
      std::max(header_painter_->GetMinimumHeaderWidth(),
               min_client_view_size.width()),
      NonClientTopBorderHeight() + min_client_view_size.height());
}

void PanelFrameView::Layout() {
  if (!header_painter_)
    return;
  header_painter_->LayoutHeader();
  header_painter_->set_header_height(NonClientTopBorderHeight());
}

void PanelFrameView::GetWindowMask(const gfx::Size&, gfx::Path*) {
  // Nothing.
}

void PanelFrameView::ResetWindowControls() {
  NOTIMPLEMENTED();
}

void PanelFrameView::UpdateWindowIcon() {
  if (!window_icon_)
    return;
  views::WidgetDelegate* delegate = frame_->widget_delegate();
  if (delegate)
    window_icon_->SetImage(delegate->GetWindowIcon());
  window_icon_->SchedulePaint();
}

void PanelFrameView::UpdateWindowTitle() {
  if (!header_painter_)
    return;
  header_painter_->SchedulePaintForTitle(title_font_list_);
}

int PanelFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!header_painter_)
    return HTNOWHERE;
  return FrameBorderHitTestController::NonClientHitTest(this,
      header_painter_.get(), point);
}

void PanelFrameView::OnPaint(gfx::Canvas* canvas) {
  if (!header_painter_)
    return;
  bool paint_as_active = ShouldPaintAsActive();
  caption_button_container_->SetPaintAsActive(paint_as_active);

  int theme_frame_id = 0;
  if (paint_as_active)
    theme_frame_id = IDR_AURA_WINDOW_HEADER_BASE_ACTIVE;
  else
    theme_frame_id = IDR_AURA_WINDOW_HEADER_BASE_INACTIVE;

  HeaderPainter::Mode header_mode = paint_as_active ?
      HeaderPainter::MODE_ACTIVE : HeaderPainter::MODE_INACTIVE;
  header_painter_->PaintHeader(canvas, header_mode, theme_frame_id, 0);
  header_painter_->PaintTitleBar(canvas, title_font_list_);
  header_painter_->PaintHeaderContentSeparator(canvas, header_mode);
}

gfx::Rect PanelFrameView::GetBoundsForClientView() const {
  if (!header_painter_)
    return bounds();
  return HeaderPainter::GetBoundsForClientView(
      NonClientTopBorderHeight(), bounds());
}

gfx::Rect PanelFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  if (!header_painter_)
    return client_bounds;
  return HeaderPainter::GetWindowBoundsForClientBounds(
      NonClientTopBorderHeight(), client_bounds);
}

}  // namespace ash
