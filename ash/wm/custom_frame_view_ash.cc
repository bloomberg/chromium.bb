// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/custom_frame_view_ash.h"

#include "ash/wm/frame_painter.h"
#include "grit/ui_resources.h"
#include "grit/ui_strings.h"  // Accessibility names
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {
// Height of non-client top border in pixels.
const int kNonClientTopBorderHeight = 10;
}  // namespace

namespace ash {
namespace internal {

// static
const char CustomFrameViewAsh::kViewClassName[] = "ash/wm/CustomFrameViewAsh";
// static
gfx::Font* CustomFrameViewAsh::title_font_ = NULL;

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
  InitClass();

  frame_ = frame;

  maximize_button_ = new views::ImageButton(this);
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

  frame_painter_->Init(frame_, window_icon_, maximize_button_, close_button_);
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
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SkBitmap* theme_bitmap = ShouldPaintAsActive() ?
      rb.GetBitmapNamed(IDR_AURA_WINDOW_HEADER_BASE_ACTIVE) :
      rb.GetBitmapNamed(IDR_AURA_WINDOW_HEADER_BASE_INACTIVE);
  frame_painter_->PaintHeader(this, canvas, theme_bitmap, NULL);
  frame_painter_->PaintTitleBar(this, canvas, *title_font_);
}

std::string CustomFrameViewAsh::GetClassName() const {
  return kViewClassName;
}

////////////////////////////////////////////////////////////////////////////////
// views::ButtonListener overrides:
void CustomFrameViewAsh::ButtonPressed(views::Button* sender,
                                       const views::Event& event) {
  if (sender == maximize_button_) {
    if (frame_->IsMaximized())
      frame_->Restore();
    else
      frame_->Maximize();
    // The maximize button may have moved out from under the cursor.
    ResetWindowControls();
  } else if (sender == close_button_) {
    frame_->Close();
  }
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, private:

int CustomFrameViewAsh::NonClientTopBorderHeight() const {
  // Reserve enough space to see the buttons, including any offset from top.
  return close_button_->bounds().bottom();
}

// static
void CustomFrameViewAsh::InitClass() {
  static bool initialized = false;
  if (!initialized)
    title_font_ = new gfx::Font(views::NativeWidgetAura::GetWindowTitleFont());
}

}  // namespace internal
}  // namespace ash
