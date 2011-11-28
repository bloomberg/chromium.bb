// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_EXTENSION_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_EXTENSION_INFOBAR_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "ui/views/controls/menu/view_menu_delegate.h"

class Browser;
namespace views {
class MenuButton;
}

class ExtensionInfoBar : public InfoBarView,
                         public ImageLoadingTracker::Observer,
                         public ExtensionInfoBarDelegate::DelegateObserver,
                         public views::ViewMenuDelegate {
 public:
  ExtensionInfoBar(Browser* browser,
                   InfoBarTabHelper* owner,
                   ExtensionInfoBarDelegate* delegate);

 private:
  virtual ~ExtensionInfoBar();

  // InfoBarView:
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    View* parent,
                                    View* child) OVERRIDE;
  virtual int ContentMinimumWidth() const OVERRIDE;

  // ImageLoadingTracker::Observer:
  virtual void OnImageLoaded(SkBitmap* image,
                             const ExtensionResource& resource,
                             int index) OVERRIDE;

  // ExtensionInfoBarDelegate::DelegateObserver:
  virtual void OnDelegateDeleted() OVERRIDE;

  // views::ViewMenuDelegate:
  virtual void RunMenu(View* source, const gfx::Point& pt) OVERRIDE;

  ExtensionInfoBarDelegate* GetDelegate();

  // TODO(pkasting): This shadows InfoBarView::delegate_.  Get rid of this once
  // InfoBars own their delegates (and thus we don't need the DelegateObserver
  // functionality).  For now, almost everyone should use GetDelegate() instead.
  InfoBarDelegate* delegate_;

  Browser* browser_;

  // The dropdown menu for accessing the contextual extension actions.
  views::MenuButton* menu_;

  // Keeps track of images being loaded on the File thread.
  ImageLoadingTracker tracker_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_EXTENSION_INFOBAR_H_
