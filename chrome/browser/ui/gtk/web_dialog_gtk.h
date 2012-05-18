// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_WEB_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_WEB_DIALOG_GTK_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/webui/web_dialog_delegate.h"
#include "chrome/browser/ui/webui/web_dialog_web_contents_delegate.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

typedef struct _GtkWidget GtkWidget;

class Browser;
class Profile;
class TabContentsContainerGtk;
class TabContentsWrapper;
class WebDialogController;

class WebDialogGtk : public WebDialogWebContentsDelegate,
                     public WebDialogDelegate {
 public:
  WebDialogGtk(Profile* profile,
               Browser* browser,
               WebDialogDelegate* delegate,
               gfx::NativeWindow parent_window);
  virtual ~WebDialogGtk();

  // Initializes the contents of the dialog (the DOMView and the callbacks).
  gfx::NativeWindow InitDialog();

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

  // Overridden from content::WebContentsDelegate:
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

 private:
  CHROMEGTK_CALLBACK_1(WebDialogGtk, void, OnResponse, int);

  // This view is a delegate to the HTML content since it needs to get notified
  // about when the dialog is closing. For all other actions (besides dialog
  // closing) we delegate to the creator of this view, which we keep track of
  // using this variable.
  WebDialogDelegate* delegate_;

  gfx::NativeWindow parent_window_;

  GtkWidget* dialog_;

  scoped_ptr<WebDialogController> dialog_controller_;
  scoped_ptr<TabContentsWrapper> tab_;
  scoped_ptr<TabContentsContainerGtk> tab_contents_container_;

  DISALLOW_COPY_AND_ASSIGN(WebDialogGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_WEB_DIALOG_GTK_H_
