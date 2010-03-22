// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INFOBARS_EXTENSION_INFOBAR_H_
#define CHROME_BROWSER_VIEWS_INFOBARS_EXTENSION_INFOBAR_H_

#include "chrome/browser/views/infobars/infobars.h"

#include "chrome/browser/views/extensions/extension_view.h"
#include "views/controls/menu/view_menu_delegate.h"

class ExtensionContextMenuModel;
class ExtensionInfoBarDelegate;

namespace views {
  class MenuButton;
  class Menu2;
}

// This class implements InfoBars for Extensions.
class ExtensionInfoBar : public InfoBar,
                         public ExtensionView::Container,
                         public views::ViewMenuDelegate {
 public:
  explicit ExtensionInfoBar(ExtensionInfoBarDelegate* delegate);
  virtual ~ExtensionInfoBar();

  // Overridden from ExtensionView::Container:
  virtual void OnExtensionMouseEvent(ExtensionView* view) {}
  virtual void OnExtensionMouseLeave(ExtensionView* view) {}
  virtual void OnExtensionPreferredSizeChanged(ExtensionView* view);

  // Overridden from views::View:
  virtual void Layout();

  // Overridden from views::ViewMenuDelegate:
  virtual void RunMenu(View* source, const gfx::Point& pt);

 private:
  // Setup the menu button showing the small extension icon and its dropdown
  // menu.
  void SetupIconAndMenu();

  NotificationRegistrar notification_registrar_;

  ExtensionInfoBarDelegate* delegate_;

  // The dropdown menu for accessing the contextual extension actions.
  scoped_ptr<ExtensionContextMenuModel> options_menu_contents_;
  scoped_ptr<views::Menu2> options_menu_menu_;
  views::MenuButton* menu_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInfoBar);
};

#endif  // CHROME_BROWSER_VIEWS_INFOBARS_EXTENSION_INFOBAR_H_
