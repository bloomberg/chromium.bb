// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_APP_LIST_DIALOG_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_APP_LIST_DIALOG_CONTAINER_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class LabelButton;
class Widget;
}

// The contents view for an App List Dialog, which covers the entire app list
// and adds a close button.
class AppListDialogContainer : public views::DialogDelegateView,
                               public views::ButtonListener {
 public:
  // Creates a new AppListDialogContainer. Takes ownership of |dialog_body|.
  AppListDialogContainer(views::View* dialog_body,
                         const base::Closure& close_callback);
  virtual ~AppListDialogContainer();

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
  views::View* dialog_body_;  // Owned by this class via the views hierarchy.
  const base::Closure close_callback_;

  views::LabelButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(AppListDialogContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_APP_LIST_DIALOG_CONTAINER_H_
