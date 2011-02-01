// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_EXTENSION_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_EXTENSION_INFOBAR_H_
#pragma once

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/ui/views/extensions/extension_view.h"
#include "views/controls/menu/view_menu_delegate.h"

class ExtensionContextMenuModel;
class ExtensionInfoBarDelegate;

namespace views {
  class MenuButton;
  class Menu2;
}

// This class implements InfoBars for Extensions.
class ExtensionInfoBar : public InfoBarView,
                         public ExtensionView::Container,
                         public ImageLoadingTracker::Observer,
                         public ExtensionInfoBarDelegate::DelegateObserver,
                         public views::ViewMenuDelegate {
 public:
  explicit ExtensionInfoBar(ExtensionInfoBarDelegate* delegate);
  virtual ~ExtensionInfoBar();

  // Overridden from ExtensionView::Container:
  virtual void OnExtensionMouseMove(ExtensionView* view) {}
  virtual void OnExtensionMouseLeave(ExtensionView* view) {}
  virtual void OnExtensionPreferredSizeChanged(ExtensionView* view);

  // Overridden from views::View:
  virtual void Layout();

  // Overridden from ImageLoadingTracker::Observer:
  virtual void OnImageLoaded(
      SkBitmap* image, ExtensionResource resource, int index);

  // Overridden from ExtensionInfoBarDelegate::DelegateObserver:
  virtual void OnDelegateDeleted();

  // Overridden from views::ViewMenuDelegate:
  virtual void RunMenu(View* source, const gfx::Point& pt);

 private:
  // Setup the menu button showing the small extension icon and its dropdown
  // menu.
  void SetupIconAndMenu();

  NotificationRegistrar notification_registrar_;

  ExtensionInfoBarDelegate* delegate_;

  // The dropdown menu for accessing the contextual extension actions.
  scoped_refptr<ExtensionContextMenuModel> options_menu_contents_;
  scoped_ptr<views::Menu2> options_menu_menu_;
  views::MenuButton* menu_;

  // Keeps track of images being loaded on the File thread.
  ImageLoadingTracker tracker_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_EXTENSION_INFOBAR_H_
