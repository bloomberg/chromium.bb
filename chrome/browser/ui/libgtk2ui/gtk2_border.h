// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTK2UI_GTK2_BORDER_H_
#define CHROME_BROWSER_UI_LIBGTK2UI_GTK2_BORDER_H_

#include "base/scoped_observer.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
class Canvas;
}

namespace views {
class LabelButton;
class LabelButtonBorder;
}

namespace libgtk2ui {
class Gtk2UI;

// Draws a gtk button border, and manages the memory of the resulting pixbufs.
class Gtk2Border : public views::Border, public ui::NativeThemeObserver {
 public:
  Gtk2Border(Gtk2UI* gtk2_ui,
             views::LabelButton* owning_button,
             scoped_ptr<views::LabelButtonBorder> border);
  virtual ~Gtk2Border();

  // Overridden from views::Border:
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Insets GetInsets() const OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;

  // Overridden from views::NativeThemeChangeObserver:
  virtual void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) OVERRIDE;

 private:
  void PaintState(const ui::NativeTheme::State state,
                  const ui::NativeTheme::ExtraParams& extra,
                  const gfx::Rect& rect,
                  gfx::Canvas* canvas);

  Gtk2UI* gtk2_ui_;

  gfx::ImageSkia button_images_[2][views::Button::STATE_COUNT];

  // The view to which we are a border. We keep track of this so that we can
  // force invalidate the layout on theme changes.
  views::LabelButton* owning_button_;

  // The views::Border that we are replacing in native mode. We keep track of
  // this for inset information.
  scoped_ptr<views::LabelButtonBorder> border_;

  ScopedObserver<ui::NativeTheme, ui::NativeThemeObserver> observer_manager_;

  DISALLOW_COPY_AND_ASSIGN(Gtk2Border);
};

}  // namespace libgtk2ui

#endif  // CHROME_BROWSER_UI_LIBGTK2UI_GTK2_BORDER_H_
