// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/custom_frame_view_ash.h"

#include "ash/wm/frame_painter.h"
#include "ash/wm/workspace/frame_maximize_button.h"
#include "grit/ash_resources.h"
#include "grit/ui_strings.h"  // Accessibility names
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/button/image_button.h"
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
const char CustomFrameViewAsh::kViewClassName[] = "ash/wm/CustomFrameViewAsh";

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, public:
CustomFrameViewAsh::CustomFrameViewAsh()
    : frame_(NULL),
      maximize_button_(NULL),
      close_button_(NULL),
      window_icon_(NULL),
      frame_painter_(new ash::FramePainter) {
}

CustomFrameViewAsh::~CustomFrameViewAsh() {
}

void CustomFrameViewAsh::Init(views::Widget* frame) {
  frame_ = frame;

  maximize_button_ = new FrameMaximizeButton(this, this);
  maximize_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MAXIMIZE));
  AddChildView(maximize_button_);
  close_button_ = new views::ImageButton(this);
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  AddChildView(close_button_);

  maximize_button_->SetVisible(frame_->widget_delegate()->CanMaximize());

  if (frame_->widget_delegate()->ShouldShowWindowIcon()) {
    window_icon_ = new views::ImageButton(this);
    AddChildView(window_icon_);
  }

  frame_painter_->Init(frame_, window_icon_, maximize_button_, close_button_,
                       FramePainter::SIZE_BUTTON_MAXIMIZES);
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
  maximize_button_->SetState(views::CustomButton::BS_NORMAL);
}

void CustomFrameViewAsh::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->SchedulePaint();
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
  bool paint_as_active = ShouldPaintAsActive();
  int theme_image_id = paint_as_active ? IDR_AURA_WINDOW_HEADER_BASE_ACTIVE :
      IDR_AURA_WINDOW_HEADER_BASE_INACTIVE;
  frame_painter_->PaintHeader(
      this,
      canvas,
      paint_as_active ? FramePainter::ACTIVE : FramePainter::INACTIVE,
      theme_image_id,
      NULL);
  frame_painter_->PaintTitleBar(this, canvas, GetTitleFont());
  frame_painter_->PaintHeaderContentSeparator(this, canvas);
}

std::string CustomFrameViewAsh::GetClassName() const {
  return kViewClassName;
}

gfx::Size CustomFrameViewAsh::GetMinimumSize() {
  return frame_painter_->GetMinimumSize(this);
}

////////////////////////////////////////////////////////////////////////////////
// views::ButtonListener overrides:
void CustomFrameViewAsh::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (event.IsShiftDown())
    ui::LayerAnimator::set_slow_animation_mode(true);
  if (sender == maximize_button_) {
    // The maximize button may move out from under the cursor.
    ResetWindowControls();
    if (frame_->IsMaximized())
      frame_->Restore();
    else
      frame_->Maximize();
    // |this| may be deleted - some windows delete their frames on maximize.
  } else if (sender == close_button_) {
    frame_->Close();
  }
  if (event.IsShiftDown())
    ui::LayerAnimator::set_slow_animation_mode(false);
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, private:

int CustomFrameViewAsh::NonClientTopBorderHeight() const {
  // Reserve enough space to see the buttons, including any offset from top and
  // reserving space for the separator line.
  return close_button_->bounds().bottom() +
      frame_painter_->HeaderContentSeparatorSize();
}

}  // namespace ash
