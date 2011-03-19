// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_WIN_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/common/content_settings.h"
#include "content/browser/tab_contents/constrained_window.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "views/controls/tree/tree_view.h"
#include "views/window/dialog_delegate.h"

class ConstrainedWindow;
class CookieInfoView;
class CookiesTreeModel;
class InfobarView;
class TabContents;

namespace views {
class Label;
class NativeButton;
}

// This is the Views implementation of the collected cookies dialog.
//
// CollectedCookiesWin is a dialog that displays the allowed and blocked
// cookies of the current tab contents. To display the dialog, invoke
// ShowCollectedCookiesDialog() on the delegate of the tab contents.
class CollectedCookiesWin : public ConstrainedDialogDelegate,
                            public NotificationObserver,
                            public views::ButtonListener,
                            public views::TabbedPaneListener,
                            public views::TreeViewController,
                            public views::View {
 public:
  // Use BrowserWindow::ShowCollectedCookiesDialog to show.
  CollectedCookiesWin(gfx::NativeWindow parent_window,
                      TabContents* tab_contents);

  // ConstrainedDialogDelegate:
  virtual std::wstring GetWindowTitle() const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

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

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  NotificationRegistrar registrar_;

  ConstrainedWindow* window_;

  // The tab contents.
  TabContents* tab_contents_;

  // Assorted views.
  views::Label* allowed_label_;
  views::Label* blocked_label_;

  views::TreeView* allowed_cookies_tree_;
  views::TreeView* blocked_cookies_tree_;

  views::NativeButton* block_allowed_button_;
  views::NativeButton* allow_blocked_button_;
  views::NativeButton* for_session_blocked_button_;

  scoped_ptr<CookiesTreeModel> allowed_cookies_tree_model_;
  scoped_ptr<CookiesTreeModel> blocked_cookies_tree_model_;

  CookieInfoView* cookie_info_view_;

  InfobarView* infobar_;

  bool status_changed_;

  DISALLOW_COPY_AND_ASSIGN(CollectedCookiesWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_WIN_H_
