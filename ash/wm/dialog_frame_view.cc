// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/dialog_frame_view.h"

#include "grit/ui_resources_standard.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

// TODO(benrg): Make frame shadow and stroke agree with the spec.

// TODO(benrg): Title bar text should be #222, not black. Text in the client
// area should also be #222, but is currently usually black. Punting on this
// until the overhaul of color handling that will be happening Real Soon Now.
const SkColor kDialogTitleColor = SK_ColorBLACK;
const SkColor kDialogBackgroundColor = SK_ColorWHITE;

// Dialog-size-dependent padding values from the spec, in pixels.
const int kGoogleSmallDialogVPadding = 16;
const int kGoogleSmallDialogHPadding = 20;
const int kGoogleMediumDialogVPadding = 28;
const int kGoogleMediumDialogHPadding = 32;
const int kGoogleLargeDialogVPadding = 30;
const int kGoogleLargeDialogHPadding = 42;

// Dialog layouts themselves contain some padding, which should be ignored in
// favor of the padding values above. Currently we hack it by nudging the
// client area outward.
const int kDialogHPaddingNudge = 13;
const int kDialogTopPaddingNudge = 5;
const int kDialogBottomPaddingNudge = 10;

// TODO(benrg): There are no examples in the spec of close boxes on medium- or
// small-size dialogs, and the close button size is not factored into other
// sizing decisions. This value could cause problems with smaller dialogs.
const int kCloseButtonSize = 44;

const gfx::Font& GetTitleFont() {
  static gfx::Font* title_font = NULL;
  if (!title_font)
    title_font = new gfx::Font(gfx::Font().DeriveFont(4, gfx::Font::NORMAL));
  return *title_font;
}

}  // namespace

namespace ash {
namespace internal {

// static
const char DialogFrameView::kViewClassName[] = "ash/wm/DialogFrameView";

////////////////////////////////////////////////////////////////////////////////
// DialogFrameView, public:

DialogFrameView::DialogFrameView() {
  set_background(views::Background::CreateSolidBackground(
      kDialogBackgroundColor));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
      rb.GetImageNamed(IDR_CLOSE_BAR).ToSkBitmap());
  close_button_->SetImage(views::CustomButton::BS_HOT,
      rb.GetImageNamed(IDR_CLOSE_BAR_H).ToSkBitmap());
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
      rb.GetImageNamed(IDR_CLOSE_BAR_P).ToSkBitmap());
  close_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                   views::ImageButton::ALIGN_MIDDLE);
  AddChildView(close_button_);
}

DialogFrameView::~DialogFrameView() {
}

////////////////////////////////////////////////////////////////////////////////
// DialogFrameView, views::NonClientFrameView:

gfx::Rect DialogFrameView::GetBoundsForClientView() const {
  gfx::Rect client_bounds = GetLocalBounds();
  client_bounds.Inset(GetClientInsets());
  return client_bounds;
}

gfx::Rect DialogFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
  window_bounds.Inset(-GetClientInsets());
  return window_bounds;
}

int DialogFrameView::NonClientHitTest(const gfx::Point& point) {
  if (close_button_->GetMirroredBounds().Contains(point))
    return HTCLOSE;
  return point.y() < GetClientInsets().top() ? HTCAPTION : HTCLIENT;
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
  title_display_rect_.Inset(GetPaddingInsets());
  title_display_rect_.set_height(GetTitleFont().GetHeight());

  // The hot rectangle for the close button is flush with the upper right of the
  // dialog. The close button image is smaller, and is centered in the hot rect.
  close_button_->SetBounds(
      width() - kCloseButtonSize,
      0, kCloseButtonSize, kCloseButtonSize);
}

void DialogFrameView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  views::WidgetDelegate* delegate = GetWidget()->widget_delegate();
  if (!delegate)
    return;
  canvas->DrawStringInt(delegate->GetWindowTitle(), GetTitleFont(),
                        kDialogTitleColor, title_display_rect_);
}

////////////////////////////////////////////////////////////////////////////////
// DialogFrameView, views::ButtonListener overrides:

void DialogFrameView::ButtonPressed(views::Button* sender,
                                    const views::Event& event) {
  if (sender == close_button_)
    GetWidget()->Close();
}

////////////////////////////////////////////////////////////////////////////////
// DialogFrameView, private:

gfx::Insets DialogFrameView::GetPaddingInsets() const {
  // These three discrete padding sizes come from the spec. If we ever wanted
  // our Google-style dialogs to be resizable, we would probably need to
  // smoothly interpolate the padding size instead.
  int v_padding, h_padding;
  if (width() <= 280) {
    v_padding = kGoogleSmallDialogVPadding;
    h_padding = kGoogleSmallDialogHPadding;
  } else if (width() <= 480) {
    v_padding = kGoogleMediumDialogVPadding;
    h_padding = kGoogleMediumDialogHPadding;
  } else {
    v_padding = kGoogleLargeDialogVPadding;
    h_padding = kGoogleLargeDialogHPadding;
  }
  return gfx::Insets(v_padding, h_padding, v_padding, h_padding);
}

gfx::Insets DialogFrameView::GetClientInsets() const {
  gfx::Insets insets = GetPaddingInsets();
  // The title should be separated from the client area by 1 em (dialog spec),
  // and one em is equal to the font size (CSS spec).
  insets += gfx::Insets(
      GetTitleFont().GetHeight() + GetTitleFont().GetFontSize() -
          kDialogTopPaddingNudge,
      -kDialogHPaddingNudge,
      -kDialogBottomPaddingNudge,
      -kDialogHPaddingNudge);
  return insets;
}

}  // namespace internal
}  // namespace views
