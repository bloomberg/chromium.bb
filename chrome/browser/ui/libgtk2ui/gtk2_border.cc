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
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/native_theme_delegate.h"

using views::Button;
using views::NativeThemeDelegate;

namespace libgtk2ui {

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
  for (int i = 0; i < 2; ++i) {
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

  if (border_->PaintsButtonState(focused, views_state)) {
    gfx::ImageSkia* image = &button_images_[focused][views_state];

    if (image->isNull() || image->size() != rect.size()) {
      *image = gfx::ImageSkia::CreateFrom1xBitmap(
          gtk2_ui_->DrawGtkButtonBorder(owning_button_->GetClassName(),
                                        state,
                                        rect.width(),
                                        rect.height()));
    }
    canvas->DrawImageInt(*image, rect.x(), rect.y());
  }
}

}  // namespace libgtk2ui
