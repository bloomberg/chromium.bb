// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/frame_painter.h"
#include "ash/wm/panel_frame_view.h"
#include "grit/ash_resources.h"
#include "grit/ui_strings.h"  // Accessibility names
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

PanelFrameView::PanelFrameView(views::Widget* frame)
    : frame_painter_(new FramePainter) {
  close_button_ = new views::ImageButton(this);
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  AddChildView(close_button_);

  minimize_button_ = new views::ImageButton(this);
  minimize_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MINIMIZE));
  AddChildView(minimize_button_);

  frame_painter_->Init(frame, NULL, minimize_button_, close_button_,
                       FramePainter::SIZE_BUTTON_MINIMIZES);
}

PanelFrameView::~PanelFrameView() {
}

void PanelFrameView::Layout() {
  frame_painter_->LayoutHeader(this, true);
}

void PanelFrameView::ResetWindowControls() {
  NOTIMPLEMENTED();
}

void PanelFrameView::UpdateWindowIcon() {
  NOTIMPLEMENTED();
}

void PanelFrameView::GetWindowMask(const gfx::Size&, gfx::Path*) {
  // Nothing.
}

int PanelFrameView::NonClientHitTest(const gfx::Point& point) {
  return frame_painter_->NonClientHitTest(this, point);
}

void PanelFrameView::OnPaint(gfx::Canvas* canvas) {
  bool paint_as_active = ShouldPaintAsActive();
  int theme_image_id = paint_as_active ? IDR_AURA_WINDOW_HEADER_BASE_ACTIVE :
      IDR_AURA_WINDOW_HEADER_BASE_INACTIVE;
  frame_painter_->PaintHeader(
      this,
      canvas,
      paint_as_active ? FramePainter::ACTIVE : FramePainter::INACTIVE,
      theme_image_id,
      NULL);
  frame_painter_->PaintHeaderContentSeparator(this, canvas);
}

gfx::Rect PanelFrameView::GetBoundsForClientView() const {
  return frame_painter_->GetBoundsForClientView(
      close_button_->bounds().bottom(),
      bounds());
}

gfx::Rect PanelFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return frame_painter_->GetWindowBoundsForClientBounds(
      close_button_->bounds().bottom(), client_bounds);
}

void PanelFrameView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  if (sender == close_button_)
    GetWidget()->Close();
}

}  // namespace ash
