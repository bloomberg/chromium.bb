// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_INCOGNITO_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_INCOGNITO_MENU_VIEW_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"

namespace views {
class LabelButton;
}

class Browser;

// TODO(https://crbug.com/934689): Separation of providing content for different
// menus and the UI effort to view it between this class and
// |ProfileMenuViewBase| is in progress.

// This bubble view is displayed when the user clicks on the avatar button in
// incognito mode and displays the incognito menu.
class IncognitoMenuView : public ProfileMenuViewBase,
                          public views::ButtonListener {
 public:
  IncognitoMenuView(views::Button* anchor_button,
                    const gfx::Rect& anchor_rect,
                    gfx::NativeView parent_window,
                    Browser* browser);
  ~IncognitoMenuView() override;

 private:
  // views::BubbleDialogDelegateView:
  void Init() override;
  base::string16 GetAccessibleWindowTitle() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void Reset();

  // Adds the incognito window count view.
  void AddIncognitoWindowCountView();

  views::LabelButton* title_card_;
  views::LabelButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(IncognitoMenuView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_INCOGNITO_MENU_VIEW_H_
