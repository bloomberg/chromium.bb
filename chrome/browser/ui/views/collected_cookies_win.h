// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Views implementation of the collected Cookies dialog.

#ifndef CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_WIN_H_
#pragma once

#include "chrome/common/content_settings.h"
#include "content/browser/tab_contents/constrained_window.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
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

// CollectedCookiesWin is a dialog that displays the allowed and blocked
// cookies of the current tab contents.  To display the dialog, invoke
// ShowCollectedCookiesDialog() on the delegate of the tab contents.

class CollectedCookiesWin : public ConstrainedDialogDelegate,
                                   NotificationObserver,
                                   views::ButtonListener,
                                   views::TabbedPane::Listener,
                                   views::TreeViewController,
                                   views::View {
 public:
  // Use BrowserWindow::ShowCollectedCookiesDialog to show.
  CollectedCookiesWin(gfx::NativeWindow parent_window,
                      TabContents* tab_contents);

  // ConstrainedDialogDelegate implementation.
  virtual std::wstring GetWindowTitle() const;
  virtual int GetDialogButtons() const;
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual void DeleteDelegate();
  virtual bool Cancel();
  virtual views::View* GetContentsView();

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::TabbedPane::Listener implementation.
  virtual void TabSelectedAt(int index);

  // views::TreeViewController implementation.
  virtual void OnTreeViewSelectionChanged(views::TreeView* tree_view);

 private:
  virtual ~CollectedCookiesWin();

  void Init();

  views::View* CreateAllowedPane();

  views::View* CreateBlockedPane();

  void EnableControls();

  void ShowCookieInfo();

  void AddContentException(views::TreeView* tree_view, ContentSetting setting);

  // Notification Observer implementation.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

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

  DISALLOW_COPY_AND_ASSIGN(CollectedCookiesWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_WIN_H_
