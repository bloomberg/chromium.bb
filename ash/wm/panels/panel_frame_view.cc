// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panels/panel_frame_view.h"

#include "ash/wm/frame_painter.h"
#include "ash/wm/workspace/frame_caption_button_container_view.h"
#include "grit/ash_resources.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
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
      title_font_(gfx::Font(views::NativeWidgetAura::GetWindowTitleFont())) {
  if (frame_type != FRAME_NONE)
    InitFramePainter();
}

PanelFrameView::~PanelFrameView() {
}

const char* PanelFrameView::GetClassName() const {
  return kViewClassName;
}

void PanelFrameView::InitFramePainter() {
  frame_painter_.reset(new FramePainter);

  caption_button_container_ = new FrameCaptionButtonContainerView(this, frame_,
      FrameCaptionButtonContainerView::MINIMIZE_ALLOWED);
  AddChildView(caption_button_container_);

  if (frame_->widget_delegate()->ShouldShowWindowIcon()) {
    window_icon_ = new views::ImageView();
    AddChildView(window_icon_);
  }

  frame_painter_->Init(frame_, window_icon_, caption_button_container_);
}

gfx::Size PanelFrameView::GetMinimumSize() {
  if (!frame_painter_)
    return gfx::Size();
  return frame_painter_->GetMinimumSize(this);
}

void PanelFrameView::Layout() {
  if (!frame_painter_)
    return;
  frame_painter_->LayoutHeader(this, true);
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
  if (!frame_painter_)
    return;
  frame_painter_->SchedulePaintForTitle(title_font_);
}

void PanelFrameView::GetWindowMask(const gfx::Size&, gfx::Path*) {
  // Nothing.
}

int PanelFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!frame_painter_)
    return HTNOWHERE;
  return frame_painter_->NonClientHitTest(this, point);
}

void PanelFrameView::OnPaint(gfx::Canvas* canvas) {
  if (!frame_painter_)
    return;
  bool paint_as_active = ShouldPaintAsActive();
  int theme_frame_id = 0;
  if (frame_painter_->ShouldUseMinimalHeaderStyle(FramePainter::THEMED_NO))
    theme_frame_id = IDR_AURA_WINDOW_HEADER_BASE_MINIMAL;
  else if (paint_as_active)
    theme_frame_id = IDR_AURA_WINDOW_HEADER_BASE_ACTIVE;
  else
    theme_frame_id = IDR_AURA_WINDOW_HEADER_BASE_INACTIVE;

  frame_painter_->PaintHeader(
      this,
      canvas,
      paint_as_active ? FramePainter::ACTIVE : FramePainter::INACTIVE,
      theme_frame_id,
      0);
  frame_painter_->PaintTitleBar(this, canvas, title_font_);
  frame_painter_->PaintHeaderContentSeparator(this, canvas);
}

gfx::Rect PanelFrameView::GetBoundsForClientView() const {
  if (!frame_painter_)
    return bounds();
  return frame_painter_->GetBoundsForClientView(
      caption_button_container_->bounds().bottom(),
      bounds());
}

gfx::Rect PanelFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  if (!frame_painter_)
    return client_bounds;
  return frame_painter_->GetWindowBoundsForClientBounds(
      caption_button_container_->bounds().bottom(), client_bounds);
}

}  // namespace ash
