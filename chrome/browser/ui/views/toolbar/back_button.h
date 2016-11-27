// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BACK_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BACK_BUTTON_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

namespace ui {
class MenuModel;
}

namespace views {
class ButtonListener;
}

// A subclass of ToolbarButton to allow the back button's hit test region to
// extend all the way to the start of the toolbar (i.e. the screen edge) in
// maximized mode, to benefit from Fitt's Law.  The button images and focus
// border are still drawn with the normal square shape.
class BackButton : public ToolbarButton {
 public:
  // Takes ownership of the |model|, which can be null if no menu
  // is to be shown.
  BackButton(Profile* profile,
             views::ButtonListener* listener,
             ui::MenuModel* model);
  ~BackButton() override;

  // Sets |margin_leading_| when the browser is maximized and updates layout
  // to make the focus rectangle centered.
  void SetLeadingMargin(int margin);

 private:
  // ToolbarButton:
  const char* GetClassName() const override;
  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override;
  gfx::Rect GetThemePaintRect() const override;

  // Any leading margin to be applied. Used when the back button is in
  // a maximized state to extend to the full window width.
  int margin_leading_;

  DISALLOW_COPY_AND_ASSIGN(BackButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BACK_BUTTON_H_
