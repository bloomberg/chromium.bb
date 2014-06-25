// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/gtk2_border.h"

#include <gtk/gtk.h>

#include "chrome/browser/ui/libgtk2ui/gtk2_ui.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_util.h"
#include "chrome/browser/ui/libgtk2ui/native_theme_gtk2.h"
#include "third_party/skia/include/effects/SkLerpXfermode.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/native_theme_delegate.h"

using views::Button;
using views::NativeThemeDelegate;

namespace libgtk2ui {

namespace {

const int kNumberOfFocusedStates = 2;

class ButtonImageSkiaSource : public gfx::ImageSkiaSource {
 public:
  ButtonImageSkiaSource(const Gtk2UI* gtk2_ui,
                        const GtkStateType state,
                        const bool focused,
                        const bool call_to_action,
                        const gfx::Size& size)
      : gtk2_ui_(gtk2_ui),
        state_(state),
        focused_(focused),
        call_to_action_(call_to_action),
        size_(size) {
  }

  virtual ~ButtonImageSkiaSource() {
  }

  virtual gfx::ImageSkiaRep GetImageForScale(float scale) OVERRIDE {
    int w = size_.width() * scale;
    int h = size_.height() * scale;
    return gfx::ImageSkiaRep(
        gtk2_ui_->DrawGtkButtonBorder(state_, focused_, call_to_action_, w, h),
        scale);
  }

 private:
  const Gtk2UI* gtk2_ui_;
  const GtkStateType state_;
  const bool focused_;
  const bool call_to_action_;
  const gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(ButtonImageSkiaSource);
};

}  // namespace

Gtk2Border::Gtk2Border(Gtk2UI* gtk2_ui,
                       views::LabelButton* owning_button,
                       scoped_ptr<views::LabelButtonBorder> border)
    : gtk2_ui_(gtk2_ui),
      owning_button_(owning_button),
      border_(border.Pass()),
      observer_manager_(this) {
  observer_manager_.Add(NativeThemeGtk2::instance());
}

Gtk2Border::~Gtk2Border() {
}

void Gtk2Border::Paint(const views::View& view, gfx::Canvas* canvas) {
  DCHECK_EQ(&view, owning_button_);
  const NativeThemeDelegate* native_theme_delegate = owning_button_;
  gfx::Rect rect(native_theme_delegate->GetThemePaintRect());
  ui::NativeTheme::ExtraParams extra;
  ui::NativeTheme::State state = native_theme_delegate->GetThemeState(&extra);

  const gfx::Animation* animation = native_theme_delegate->GetThemeAnimation();
  if (animation && animation->is_animating()) {
    // Linearly interpolate background and foreground painters during animation.
    const SkRect sk_rect = gfx::RectToSkRect(rect);
    canvas->sk_canvas()->saveLayer(&sk_rect, NULL);
    state = native_theme_delegate->GetBackgroundThemeState(&extra);
    PaintState(state, extra, rect, canvas);

    SkPaint paint;
    skia::RefPtr<SkXfermode> sk_lerp_xfer =
        skia::AdoptRef(SkLerpXfermode::Create(animation->GetCurrentValue()));
    paint.setXfermode(sk_lerp_xfer.get());
    canvas->sk_canvas()->saveLayer(&sk_rect, &paint);
    state = native_theme_delegate->GetForegroundThemeState(&extra);
    PaintState(state, extra, rect, canvas);
    canvas->sk_canvas()->restore();

    canvas->sk_canvas()->restore();
  } else {
    PaintState(state, extra, rect, canvas);
  }
}

gfx::Insets Gtk2Border::GetInsets() const {
  return border_->GetInsets();
}

gfx::Size Gtk2Border::GetMinimumSize() const {
  return border_->GetMinimumSize();
}

void Gtk2Border::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  DCHECK_EQ(observed_theme, NativeThemeGtk2::instance());
  for (int i = 0; i < kNumberOfFocusedStates; ++i) {
    for (int j = 0; j < views::Button::STATE_COUNT; ++j) {
      button_images_[i][j] = gfx::ImageSkia();
    }
  }

  // Our owning view must have its layout invalidated because the insets could
  // have changed.
  owning_button_->InvalidateLayout();
}

void Gtk2Border::PaintState(const ui::NativeTheme::State state,
                            const ui::NativeTheme::ExtraParams& extra,
                            const gfx::Rect& rect,
                            gfx::Canvas* canvas) {
  bool focused = extra.button.is_focused;
  Button::ButtonState views_state = Button::GetButtonStateFrom(state);

  if (border_->GetPainter(focused, views_state) ||
      (focused && border_->GetPainter(false, views_state))) {
    gfx::ImageSkia* image = &button_images_[focused][views_state];

    if (image->isNull() || image->size() != rect.size()) {
      bool call_to_action = owning_button_->GetClassName() ==
          views::BlueButton::kViewClassName;
      GtkStateType gtk_state = GetGtkState(state);
      *image = gfx::ImageSkia(
          new ButtonImageSkiaSource(gtk2_ui_,
                                    gtk_state,
                                    focused,
                                    call_to_action,
                                    rect.size()),
          rect.size());
    }
    canvas->DrawImageInt(*image, rect.x(), rect.y());
  }
}

}  // namespace libgtk2ui
