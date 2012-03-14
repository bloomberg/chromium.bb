// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panel_frame_view.h"

#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/aura/cursor.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

const int kClientViewPaddingLeft = 0;
const int kClientViewPaddingRight = 0;
const int kClientViewPaddingBottom = 0;
const int kSeparatorWidth = 1;
const SkColor kCaptionColor = SK_ColorGRAY;
const SkColor kCaptionIconSeparatorColor = SkColorSetRGB(0x55, 0x55, 0x55);

}  // namespace

// Buttons for panel control
class PanelControlButton : public views::CustomButton {
 public:
  PanelControlButton(views::ButtonListener* listener,
                     const SkBitmap& icon)
      : views::CustomButton(listener),
        icon_(icon) {
  }
  virtual ~PanelControlButton() {}

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    SkPaint paint;
    paint.setAlpha(hover_animation_->CurrentValueBetween(80, 255));
    canvas->DrawBitmapInt(icon_,
                          (width() - icon_.width())/2,
                          (height() - icon_.height())/2, paint);
  }
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size(icon_.width(), icon_.height());
    size.Enlarge(3, 2);
    return size;
  }

 private:
  SkBitmap icon_;

  DISALLOW_COPY_AND_ASSIGN(PanelControlButton);
};


class PanelCaption : public views::View,
                     public views::ButtonListener {
 public:
  PanelCaption() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    close_button_ = new PanelControlButton(this,
        *rb.GetImageNamed(IDR_AURA_WINDOW_CLOSE_ICON).ToSkBitmap());
    AddChildView(close_button_);
  }
  virtual ~PanelCaption() {}

  // point is in parent's coordinates
  int NonClientHitTest(const gfx::Point& point) {
    if (!GetLocalBounds().Contains(point))
      return HTNOWHERE;

    gfx::Point translated_point(point);
    View::ConvertPointToView(parent(), this, &translated_point);
    if (close_button_->GetMirroredBounds().Contains(translated_point))
      return HTCLOSE;
    return HTCAPTION;
  }

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(0, close_button_->GetPreferredSize().height());
  }


 private:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(GetLocalBounds(), kCaptionColor);
    gfx::Rect separator(close_button_->bounds());
    separator.set_width(kSeparatorWidth);
    canvas->FillRect(separator, kCaptionIconSeparatorColor);
  }

  virtual void Layout() OVERRIDE {
    gfx::Size close_ps = close_button_->GetPreferredSize();
    close_button_->SetBoundsRect(
        gfx::Rect(width() - close_ps.width(), 0,
                  close_ps.width(), close_ps.height()));
  }

  // Overriden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    if (sender == close_button_)
      GetWidget()->Close();
  }

  views::Button* close_button_;

  DISALLOW_COPY_AND_ASSIGN(PanelCaption);
};

PanelFrameView::PanelFrameView()
    : panel_caption_(new PanelCaption) {
  AddChildView(panel_caption_);
}

PanelFrameView::~PanelFrameView() {
}

int PanelFrameView::CaptionHeight() const {
  return panel_caption_->GetPreferredSize().height();
}

void PanelFrameView::Layout() {
  client_view_bounds_ = GetLocalBounds();
  client_view_bounds_.Inset(kClientViewPaddingLeft,
                            CaptionHeight(),
                            kClientViewPaddingRight,
                            kClientViewPaddingBottom);

  panel_caption_->SetBounds(0, 0, width(), CaptionHeight());
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
  if (!GetLocalBounds().Contains(point))
    return HTNOWHERE;

  gfx::Point translated_point(point);
  View::ConvertPointToView(parent(), this, &translated_point);

  int client_view_result =
      GetWidget()->client_view()->NonClientHitTest(translated_point);
  if (client_view_result != HTNOWHERE)
    return client_view_result;

  int caption_result = panel_caption_->NonClientHitTest(translated_point);
  if (caption_result != HTNOWHERE)
    return caption_result;
  return HTNOWHERE;
}

gfx::Rect PanelFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect PanelFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
  window_bounds.Inset(-kClientViewPaddingLeft,
                      -CaptionHeight(),
                      -kClientViewPaddingRight,
                      -kClientViewPaddingBottom);
  return window_bounds;
}

}  // namespace ash
