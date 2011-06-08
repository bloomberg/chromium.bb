// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HTML_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_HTML_DIALOG_VIEW_H_
#pragma once

#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/browser/ui/webui/html_dialog_tab_contents_delegate.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/gfx/size.h"
#include "views/window/window_delegate.h"

class Browser;
namespace views {
class Window;
}

////////////////////////////////////////////////////////////////////////////////
//
// HtmlDialogView is a view used to display an HTML dialog to the user. The
// content of the dialogs is determined by the delegate
// (HtmlDialogUIDelegate), but is basically a file URL along with a
// JSON input string. The HTML is supposed to show a UI to the user and is
// expected to send back a JSON file as a return value.
//
////////////////////////////////////////////////////////////////////////////////
//
// TODO(akalin): Make HtmlDialogView contain an HtmlDialogTabContentsDelegate
// instead of inheriting from it to avoid violating the "no multiple
// inheritance" rule.
class HtmlDialogView
    : public DOMView,
      public HtmlDialogTabContentsDelegate,
      public HtmlDialogUIDelegate,
      public views::WindowDelegate,
      public NotificationObserver {
 public:
  HtmlDialogView(Profile* profile, HtmlDialogUIDelegate* delegate);
  virtual ~HtmlDialogView();

  // Initializes the contents of the dialog (the DOMView and the callbacks).
  void InitDialog();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

  // Overridden from views::WindowDelegate:
  virtual bool CanResize() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual views::View* GetContentsView();
  virtual views::View* GetInitiallyFocusedView();
  virtual bool ShouldShowWindowTitle() const;

  // Overridden from HtmlDialogUIDelegate:
  virtual bool IsDialogModal() const;
  virtual std::wstring GetDialogTitle() const;
  virtual GURL GetDialogContentURL() const;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const;
  virtual void GetDialogSize(gfx::Size* size) const;
  virtual std::string GetDialogArgs() const;
  virtual void OnWindowClosed();
  virtual void OnDialogClosed(const std::string& json_retval);
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog);
  virtual bool ShouldShowDialogTitle() const;
  virtual bool HandleContextMenu(const ContextMenuParams& params);

  // Overridden from TabContentsDelegate:
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event);
  virtual void CloseContents(TabContents* source);

  // Overridden from NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  // Register accelerators for this dialog.
  virtual void RegisterDialogAccelerators();

 private:
  FRIEND_TEST_ALL_PREFIXES(HtmlDialogBrowserTest, TestStateTransition);

  // A state used to ensure that we show the window only after the
  // renderer painted the full page.
  enum DialogState {
    NONE,
    INITIALIZED,  // FreezeUpdates property is set to prevent WM from showing
                  // the window until the property is remoevd.
    LOADED,       // Renderer loaded the page.
    PAINTED,      // 1st paint event after the page is loaded.
                  // FreezeUpdates property is removed to tell WM to shows
                  // the window.
  };
  DialogState state_;

  // This view is a delegate to the HTML content since it needs to get notified
  // about when the dialog is closing. For all other actions (besides dialog
  // closing) we delegate to the creator of this view, which we keep track of
  // using this variable.
  HtmlDialogUIDelegate* delegate_;

  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(HtmlDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HTML_DIALOG_VIEW_H_
