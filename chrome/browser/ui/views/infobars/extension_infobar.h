// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_EXTENSION_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_EXTENSION_INFOBAR_H_

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "ui/views/controls/button/menu_button_listener.h"

class Browser;
namespace views {
class ImageView;
class MenuButton;
}

class ExtensionInfoBar : public InfoBarView,
                         public ExtensionInfoBarDelegate::DelegateObserver,
                         public views::MenuButtonListener {
 public:
  ExtensionInfoBar(Browser* browser,
                   InfoBarService* owner,
                   ExtensionInfoBarDelegate* delegate);

 private:
  virtual ~ExtensionInfoBar();

  // InfoBarView:
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;
  virtual int ContentMinimumWidth() const OVERRIDE;

  // ExtensionInfoBarDelegate::DelegateObserver:
  virtual void OnDelegateDeleted() OVERRIDE;

  // views::MenuButtonListener:
  virtual void OnMenuButtonClicked(views::View* source,
                                   const gfx::Point& point) OVERRIDE;

  void OnImageLoaded(const gfx::Image& image);

  ExtensionInfoBarDelegate* GetDelegate();

  // TODO(pkasting): This shadows InfoBarView::delegate_.  Get rid of this once
  // InfoBars own their delegates (and thus we don't need the DelegateObserver
  // functionality).  For now, almost everyone should use GetDelegate() instead.
  InfoBarDelegate* delegate_;

  Browser* browser_;

  // The infobar icon used for the extension infobar. The icon can be either a
  // plain image (in which case |icon_as_image_| is set) or a dropdown menu (in
  // which case |icon_as_menu_| is set).
  // The icon is a dropdown menu if the extension showing the infobar shows
  // configure context menus.
  views::View* infobar_icon_;

  // The dropdown menu for accessing the contextual extension actions.
  // It is non-NULL if the |infobar_icon_| is a menu button and in that case
  // |icon_as_menu_ == infobar_icon_|.
  views::MenuButton* icon_as_menu_;

  // The image view for the icon.
  // It is non-NULL if |infobar_icon_| is an image and in that case
  // |icon_as_image_ == infobar_icon_|.
  views::ImageView* icon_as_image_;

  base::WeakPtrFactory<ExtensionInfoBar> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_EXTENSION_INFOBAR_H_
