// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_APP_LIST_DIALOG_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_APP_LIST_DIALOG_CONTENTS_VIEW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

class AppListControllerDelegate;

namespace views {
class LabelButton;
class Widget;
}

// The contents view for an App List Dialog, which covers the entire app list
// and adds a close button.
class AppListDialogContentsView : public views::DialogDelegateView,
                                  public views::ButtonListener {
 public:
  AppListDialogContentsView(
      AppListControllerDelegate* app_list_controller_delegate,
      views::View* dialog_body);
  virtual ~AppListDialogContentsView();

  // Create a |dialog| window Widget with the specified |parent|. The dialog
  // will be resized to fill the body of the app list.
  static views::Widget* CreateDialogWidget(gfx::NativeView parent,
                                           const gfx::Rect& bounds,
                                           AppListDialogContentsView* dialog);

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::ClientView* CreateClientView(views::Widget* widget) OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 protected:
  // Overridden from views::WidgetDelegate:
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual void WindowClosing() OVERRIDE;

 private:
  // Weak. Owned by the AppListService singleton.
  AppListControllerDelegate* app_list_controller_delegate_;

  views::View* dialog_body_;
  views::LabelButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(AppListDialogContentsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_APP_LIST_DIALOG_CONTENTS_VIEW_H_
