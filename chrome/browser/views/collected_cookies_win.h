// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Views implementation of the collected Cookies dialog.

#ifndef CHROME_BROWSER_VIEWS_COLLECTED_COOKIES_WIN_H_
#define CHROME_BROWSER_VIEWS_COLLECTED_COOKIES_WIN_H_

#include "chrome/browser/tab_contents/constrained_window.h"
#include "chrome/common/notification_registrar.h"
#include "views/window/dialog_delegate.h"

class ConstrainedWindow;
class CookiesTreeModel;
class TabContents;
namespace views {
class Label;
class TreeView;
}

// CollectedCookiesWin is a dialog that displays the allowed and blocked
// cookies of the current tab contents.  To display the dialog, invoke
// ShowCollectedCookiesDialog() on the delegate of the tab contents.

class CollectedCookiesWin : public ConstrainedDialogDelegate,
                                   NotificationObserver,
                                   views::View {
 public:
  // Use BrowserWindow::ShowCollectedCookiesDialog to show.
  CollectedCookiesWin(gfx::NativeWindow parent_window,
                      TabContents* tab_contents);

  // views::DialogDelegate implementation.
  virtual std::wstring GetWindowTitle() const;
  virtual int GetDialogButtons() const;
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual void DeleteDelegate();

  virtual bool Cancel();

  // views::WindowDelegate implementation.
  virtual views::View* GetContentsView();

  // views::View implementation.
  gfx::Size GetPreferredSize();

 private:
  virtual ~CollectedCookiesWin();

  void Init();

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

  scoped_ptr<CookiesTreeModel> allowed_cookies_tree_model_;
  scoped_ptr<CookiesTreeModel> blocked_cookies_tree_model_;

  DISALLOW_COPY_AND_ASSIGN(CollectedCookiesWin);
};

#endif  // CHROME_BROWSER_VIEWS_COLLECTED_COOKIES_WIN_H_
