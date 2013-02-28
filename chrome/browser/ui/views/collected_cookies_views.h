// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_VIEWS_H_

#include "base/compiler_specific.h"
#include "chrome/common/content_settings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/controls/tree/tree_view_controller.h"
#include "ui/views/window/dialog_delegate.h"

class CookieInfoView;
class CookiesTreeModel;
class InfobarView;

namespace content {
class WebContents;
}

namespace views {
class Label;
class TextButton;
class TreeView;
class Widget;
}

// This is the Views implementation of the collected cookies dialog.
//
// CollectedCookiesViews is a dialog that displays the allowed and blocked
// cookies of the current tab contents. To display the dialog, invoke
// ShowCollectedCookiesDialog() on the delegate of the WebContents's
// content settings tab helper.
class CollectedCookiesViews : public views::DialogDelegateView,
                              public content::NotificationObserver,
                              public views::ButtonListener,
                              public views::TabbedPaneListener,
                              public views::TreeViewController {
 public:
  // Use BrowserWindow::ShowCollectedCookiesDialog to show.
  explicit CollectedCookiesViews(content::WebContents* web_contents);

  // views::DialogDelegate:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::TabbedPaneListener:
  virtual void TabSelectedAt(int index) OVERRIDE;

  // views::TreeViewController:
  virtual void OnTreeViewSelectionChanged(views::TreeView* tree_view) OVERRIDE;

  // views::View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;

 private:
  virtual ~CollectedCookiesViews();

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

  views::Widget* window_;

  // The web contents.
  content::WebContents* web_contents_;

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

  DISALLOW_COPY_AND_ASSIGN(CollectedCookiesViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_COLLECTED_COOKIES_VIEWS_H_
