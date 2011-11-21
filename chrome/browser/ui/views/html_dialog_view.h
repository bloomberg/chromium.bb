// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HTML_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_HTML_DIALOG_VIEW_H_
#pragma once

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/tab_first_render_watcher.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/browser/ui/webui/html_dialog_tab_contents_delegate.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "ui/gfx/size.h"
#include "views/widget/widget_delegate.h"

class Browser;

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
      public views::WidgetDelegate,
      public TabFirstRenderWatcher::Delegate {
 public:
  HtmlDialogView(Profile* profile, HtmlDialogUIDelegate* delegate);
  virtual ~HtmlDialogView();

  // Initializes the contents of the dialog (the DOMView and the callbacks).
  void InitDialog();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator)
      OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child)
      OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual bool CanResize() const OVERRIDE;
  virtual bool IsModal() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual bool ShouldShowWindowTitle() const OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // Overridden from HtmlDialogUIDelegate:
  virtual bool IsDialogModal() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog)
      OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;

  // Overridden from TabContentsDelegate:
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) OVERRIDE;
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event)
      OVERRIDE;
  virtual void CloseContents(TabContents* source) OVERRIDE;

 protected:
  // Register accelerators for this dialog.
  virtual void RegisterDialogAccelerators();

  // TabFirstRenderWatcher::Delegate implementation.
  virtual void OnRenderHostCreated(RenderViewHost* host) OVERRIDE;
  virtual void OnTabMainFrameLoaded() OVERRIDE;
  virtual void OnTabMainFrameFirstRender() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(HtmlDialogBrowserTest, WebContentRendered);

  // Whether the view is initialized. That is, dialog acceleartors is registered
  // and FreezeUpdates property is set to prevent WM from showing the window
  // until the property is removed.
  bool initialized_;

  // Watches for TabContents rendering.
  scoped_ptr<TabFirstRenderWatcher> tab_watcher_;

  // This view is a delegate to the HTML content since it needs to get notified
  // about when the dialog is closing. For all other actions (besides dialog
  // closing) we delegate to the creator of this view, which we keep track of
  // using this variable.
  HtmlDialogUIDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(HtmlDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HTML_DIALOG_VIEW_H_
