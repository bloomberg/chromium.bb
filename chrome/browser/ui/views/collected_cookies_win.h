// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_WIN_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/common/content_settings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/window/dialog_delegate.h"

class ConstrainedWindow;
class CookieInfoView;
class CookiesTreeModel;
class InfobarView;
class TabContents;
class TabContentsWrapper;

namespace views {
class Label;
class TextButton;
}

// This is the Views implementation of the collected cookies dialog.
//
// CollectedCookiesWin is a dialog that displays the allowed and blocked
// cookies of the current tab contents. To display the dialog, invoke
// ShowCollectedCookiesDialog() on the delegate of the tab contents wrapper's
// content settings tab helper.
class CollectedCookiesWin : public views::DialogDelegate,
                            public content::NotificationObserver,
                            public views::ButtonListener,
                            public views::TabbedPaneListener,
                            public views::TreeViewController,
                            public views::View {
 public:
  // Use BrowserWindow::ShowCollectedCookiesDialog to show.
  CollectedCookiesWin(gfx::NativeWindow parent_window,
                      TabContentsWrapper* wrapper);

  // views::DialogDelegate:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::TabbedPaneListener:
  virtual void TabSelectedAt(int index);

  // views::TreeViewController:
  virtual void OnTreeViewSelectionChanged(views::TreeView* tree_view);

 private:
  virtual ~CollectedCookiesWin();

  void Init();

  views::View* CreateAllowedPane();

  views::View* CreateBlockedPane();

  void EnableControls();

  void ShowCookieInfo();

  void AddContentException(views::TreeView* tree_view, ContentSetting setting);

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;

  ConstrainedWindow* window_;

  // The tab contents.
  TabContentsWrapper* wrapper_;

  // Assorted views.
  views::Label* allowed_label_;
  views::Label* blocked_label_;

  views::TreeView* allowed_cookies_tree_;
  views::TreeView* blocked_cookies_tree_;

  views::TextButton* block_allowed_button_;
  views::TextButton* allow_blocked_button_;
  views::TextButton* for_session_blocked_button_;

  scoped_ptr<CookiesTreeModel> allowed_cookies_tree_model_;
  scoped_ptr<CookiesTreeModel> blocked_cookies_tree_model_;

  CookieInfoView* cookie_info_view_;

  InfobarView* infobar_;

  bool status_changed_;

  DISALLOW_COPY_AND_ASSIGN(CollectedCookiesWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_WIN_H_
