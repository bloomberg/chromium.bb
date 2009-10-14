// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BROWSER_EXTENDER_H_
#define CHROME_BROWSER_CHROMEOS_BROWSER_EXTENDER_H_

#include "views/controls/button/button.h"
#include "views/controls/menu/simple_menu_model.h"
#include "views/controls/menu/view_menu_delegate.h"

class BrowserView;
class StatusAreaView;

namespace views {
class ImageButton;
class SimpleMenuModel;
}

namespace gfx {
class Point;
}

// BrowserExtender adds extra controls to BrowserView for ChromeOS.
// This layout controls as follows
//                  ____  __ __
//      [MainMenu] /    \   \  \     [StatusArea]
//
// and adds the system context menu to the remaining arae of the titlebar.
class BrowserExtender : public views::ButtonListener,
                        public views::ContextMenuController {
 public:
  explicit BrowserExtender(BrowserView* browser_view);
  virtual ~BrowserExtender() {}

  // Initializes the extender. This creates controls, status area and
  // menus.
  void Init();

  // Layouts controls within the given bounds and returns the remaining
  // bounds for tabstip to be layed out.
  gfx::Rect Layout(const gfx::Rect& bounds);

  // Tests if the given |point|, which is given in window coordinates,
  // hits any of controls.
  bool NonClientHitTest(const gfx::Point& point);

 private:
  // Creates system menu.
  void InitSystemMenu();

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::ContextMenuController overrides:
  virtual void ShowContextMenu(views::View* source,
                               int x,
                               int y,
                               bool is_mouse_gesture);

  // BrowserView to be extended.
  BrowserView* browser_view_;  // weak

  // Main menu button.
  views::ImageButton* main_menu_;

  // Status Area view.
  StatusAreaView* status_area_;

  // System menus
  scoped_ptr<views::SimpleMenuModel> system_menu_contents_;
  scoped_ptr<views::Menu2> system_menu_menu_;

  DISALLOW_COPY_AND_ASSIGN(BrowserExtender);
};

#endif  // CHROME_BROWSER_CHROMEOS_BROWSER_EXTENDER_H_

