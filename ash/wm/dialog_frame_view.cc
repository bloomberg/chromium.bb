// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/dialog_frame_view.h"

#include "ui/base/hit_test.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace internal {

// static
const char DialogFrameView::kViewClassName[] = "ash/wm/DialogFrameView";

// static
gfx::Font* DialogFrameView::title_font_ = NULL;

// TODO(benrg): tweak these values until they match Google-style.
// TODO(benrg): this may also mean tweaking the frame shadow opacity.
const SkColor kDialogBackgroundColor = SK_ColorWHITE;
// const SkColor kDialogBorderColor = SkColorSetRGB(0xCC, 0xCC, 0xCC);
const SkColor kDialogTitleColor = SK_ColorBLACK;

// TODO(benrg): Replace with width-based padding heuristic.
// |kDialogPadding| is the standardized padding amount, the specific per-side
// padding values are offset from this to achieve a uniform look with our
// existing code.
static int kDialogPadding = 30;
static int kDialogHPadding = kDialogPadding - 13;
static int kDialogTopPadding = kDialogPadding;
static int kDialogBottomPadding = kDialogPadding - 10;
static int kDialogContentVSpacing = 5;

const int kCloseButtonSize = 16;

////////////////////////////////////////////////////////////////////////////////
// DialogFrameView, public:

DialogFrameView::DialogFrameView() {
  set_background(views::Background::CreateSolidBackground(
      kDialogBackgroundColor));
  if (!title_font_)
    title_font_ = new gfx::Font(gfx::Font().DeriveFont(4, gfx::Font::NORMAL));
}

DialogFrameView::~DialogFrameView() {
}

////////////////////////////////////////////////////////////////////////////////
// DialogFrameView, views::NonClientFrameView:

gfx::Rect DialogFrameView::GetBoundsForClientView() const {
  gfx::Rect client_bounds = GetLocalBounds();
  client_bounds.Inset(kDialogHPadding, GetNonClientTopHeight(),
                      kDialogHPadding, kDialogBottomPadding);
  return client_bounds;
}

gfx::Rect DialogFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
  window_bounds.Inset(-kDialogHPadding, -GetNonClientTopHeight(),
                      -kDialogHPadding, -kDialogBottomPadding);
  return window_bounds;
}

int DialogFrameView::NonClientHitTest(const gfx::Point& point) {
  if (close_button_rect_.Contains(point))
    return HTCLOSE;
  return point.y() < GetNonClientTopHeight() ? HTCAPTION : HTCLIENT;
}

void DialogFrameView::GetWindowMask(const gfx::Size& size,
                                    gfx::Path* window_mask) {
  // Nothing to do.
}

void DialogFrameView::ResetWindowControls() {
  // Nothing to do.
}

void DialogFrameView::UpdateWindowIcon() {
  // Nothing to do.
}

////////////////////////////////////////////////////////////////////////////////
// DialogFrameView, views::View overrides:

std::string DialogFrameView::GetClassName() const {
  return kViewClassName;
}

void DialogFrameView::Layout() {
  title_display_rect_ = GetLocalBounds();
  // TODO(benrg): size based on font height rather than bottom padding.
  close_button_rect_.SetRect(width() - kDialogPadding - kCloseButtonSize,
                             kDialogTopPadding, kCloseButtonSize,
                             kCloseButtonSize);
  title_display_rect_.Inset(kDialogPadding, kDialogTopPadding,
                            kDialogPadding + kCloseButtonSize,
                            kDialogBottomPadding);
  title_display_rect_.set_height(title_font_->GetHeight());
}

void DialogFrameView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  canvas->FillRect(SK_ColorRED, close_button_rect_);
  views::WidgetDelegate* delegate = GetWidget()->widget_delegate();
  if (!delegate)
    return;
  canvas->DrawStringInt(delegate->GetWindowTitle(), *title_font_,
                        kDialogTitleColor, title_display_rect_);
}

// TODO(benrg): You may want to use a views::Button for the close box instead.
bool DialogFrameView::OnMousePressed(const views::MouseEvent& event) {
  if (close_button_rect_.Contains(event.location()))
    return true;
  return View::OnMousePressed(event);
}
void DialogFrameView::OnMouseReleased(const views::MouseEvent& event) {
  if (close_button_rect_.Contains(event.location()))
    GetWidget()->Close();
}

////////////////////////////////////////////////////////////////////////////////
// DialogFrameView, private:

int DialogFrameView::GetNonClientTopHeight() const {
  return kDialogTopPadding + title_font_->GetHeight() + kDialogContentVSpacing;
}

}  // namespace internal
}  // namespace views
