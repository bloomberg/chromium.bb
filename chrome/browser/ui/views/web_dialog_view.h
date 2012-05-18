// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_DIALOG_VIEW_H_
#pragma once

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/tab_render_watcher.h"
#include "chrome/browser/ui/webui/web_dialog_delegate.h"
#include "chrome/browser/ui/webui/web_dialog_web_contents_delegate.h"
#include "ui/gfx/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

class Browser;
class WebDialogController;
class Profile;

namespace views {
class WebView;
}

////////////////////////////////////////////////////////////////////////////////
//
// WebDialogView is a view used to display an web dialog to the user. The
// content of the dialogs is determined by the delegate
// (WebDialogDelegate), but is basically a file URL along with a
// JSON input string. The HTML is supposed to show a UI to the user and is
// expected to send back a JSON file as a return value.
//
////////////////////////////////////////////////////////////////////////////////
//
// TODO(akalin): Make WebDialogView contain an WebDialogWebContentsDelegate
// instead of inheriting from it to avoid violating the "no multiple
// inheritance" rule.
// TODO(beng): This class should not depend on Browser or Profile, only
//             content::BrowserContext.
class WebDialogView
    : public views::View,
      public WebDialogWebContentsDelegate,
      public WebDialogDelegate,
      public views::WidgetDelegate,
      public TabRenderWatcher::Delegate {
 public:
  WebDialogView(Profile* profile,
                Browser* browser,
                WebDialogDelegate* delegate);
  virtual ~WebDialogView();

  // For testing.
  content::WebContents* web_contents();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator)
      OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual bool CanResize() const OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual std::string GetWindowName() const OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual bool ShouldShowWindowTitle() const OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // Overridden from WebDialogDelegate:
  virtual ui::ModalType GetDialogModalType() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual void GetMinimumDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(content::WebContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;

  // Overridden from content::WebContentsDelegate:
  virtual void MoveContents(content::WebContents* source,
                            const gfx::Rect& pos) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual void CloseContents(content::WebContents* source) OVERRIDE;
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) OVERRIDE;
  virtual void LoadingStateChanged(content::WebContents* source) OVERRIDE;

 protected:
  // Overridden from TabRenderWatcher::Delegate:
  virtual void OnRenderHostCreated(content::RenderViewHost* host) OVERRIDE;
  virtual void OnTabMainFrameLoaded() OVERRIDE;
  virtual void OnTabMainFrameRender() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebDialogBrowserTest, WebContentRendered);

  // Initializes the contents of the dialog.
  void InitDialog();

  // Whether the view is initialized. That is, dialog accelerators is registered
  // and FreezeUpdates property is set to prevent WM from showing the window
  // until the property is removed.
  bool initialized_;

  // Watches for WebContents rendering.
  scoped_ptr<TabRenderWatcher> tab_watcher_;

  // This view is a delegate to the HTML content since it needs to get notified
  // about when the dialog is closing. For all other actions (besides dialog
  // closing) we delegate to the creator of this view, which we keep track of
  // using this variable.
  WebDialogDelegate* delegate_;

  // Controls lifetime of dialog.
  scoped_ptr<WebDialogController> dialog_controller_;

  views::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(WebDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_DIALOG_VIEW_H_
