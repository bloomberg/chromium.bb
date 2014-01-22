// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/gtk2_border.h"

#include <gtk/gtk.h>

#include "chrome/browser/ui/libgtk2ui/gtk2_ui.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/views/controls/button/custom_button.h"

namespace libgtk2ui {

namespace {

GtkStateType ViewsToGtkState(views::Button::ButtonState state) {
  switch (state) {
    case views::Button::STATE_HOVERED:
      return GTK_STATE_PRELIGHT;
    case views::Button::STATE_PRESSED:
      return GTK_STATE_ACTIVE;
    case views::Button::STATE_DISABLED:
      return GTK_STATE_INSENSITIVE;
    default:
      return GTK_STATE_NORMAL;
  }
}

class ButtonImageSkiaSource : public gfx::ImageSkiaSource {
 public:
  ButtonImageSkiaSource(const Gtk2UI* gtk2_ui,
                        GtkStateType state,
                        const gfx::Size& size)
      : gtk2_ui_(gtk2_ui),
        state_(state),
        size_(size) {
  }

  virtual ~ButtonImageSkiaSource() {
  }

  virtual gfx::ImageSkiaRep GetImageForScale(float scale) OVERRIDE {
    int w = size_.width() * scale;
    int h = size_.height() * scale;
    return gfx::ImageSkiaRep(
        gtk2_ui_->DrawGtkButtonBorder(state_, w, h), scale);
  }

 private:
  const Gtk2UI* gtk2_ui_;
  GtkStateType state_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(ButtonImageSkiaSource);
};

}  // namespace

Gtk2Border::Gtk2Border(Gtk2UI* gtk2_ui,
                       views::CustomButton* owning_button,
                       views::Border* border)
    : gtk2_ui_(gtk2_ui),
      use_gtk_(gtk2_ui_->GetUseSystemTheme()),
      owning_button_(owning_button),
      border_(border) {
  gtk2_ui_->AddGtkBorder(this);
}

Gtk2Border::~Gtk2Border() {
  gtk2_ui_->RemoveGtkBorder(this);
}

void Gtk2Border::InvalidateAndSetUsesGtk(bool use_gtk) {
  hovered_ = gfx::ImageSkia();
  pressed_ = gfx::ImageSkia();

  // Our owning view must have its layout invalidated because the insets could
  // have changed.
  owning_button_->InvalidateLayout();

  use_gtk_ = use_gtk;
}

void Gtk2Border::Paint(const views::View& view, gfx::Canvas* canvas) {
  if (!use_gtk_) {
    border_->Paint(view, canvas);
    return;
  }

  DCHECK_EQ(&view, owning_button_);
  GtkStateType state = ViewsToGtkState(owning_button_->state());

  if (state == GTK_STATE_PRELIGHT) {
    if (hovered_.isNull() || hovered_.size() != view.size()) {
      hovered_ = gfx::ImageSkia(
          new ButtonImageSkiaSource(gtk2_ui_, state, view.size()), view.size());
    }

    canvas->DrawImageInt(hovered_, 0, 0);
  } else if (state == GTK_STATE_ACTIVE) {
    if (pressed_.isNull() || pressed_.size() != view.size()) {
      pressed_ = gfx::ImageSkia(
          new ButtonImageSkiaSource(gtk2_ui_, state, view.size()), view.size());
    }

    canvas->DrawImageInt(pressed_, 0, 0);
  }
}

gfx::Insets Gtk2Border::GetInsets() const {
  if (!use_gtk_)
    return border_->GetInsets();

  return gtk2_ui_->GetButtonInsets();
}

gfx::Size Gtk2Border::GetMinimumSize() const {
  if (!use_gtk_)
    return border_->GetMinimumSize();

  gfx::Insets insets = GetInsets();
  return gfx::Size(insets.width(), insets.height());
}

}  // namespace libgtk2ui
