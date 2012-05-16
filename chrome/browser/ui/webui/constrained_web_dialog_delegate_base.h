// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_DELEGATE_BASE_H_
#define CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_DELEGATE_BASE_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/web_dialog_web_contents_delegate.h"
#include "chrome/browser/ui/webui/web_dialog_ui.h"

// Platform-agnostic base implementation of ConstrainedWebDialogDelegate.
class ConstrainedWebDialogDelegateBase
    : public ConstrainedWebDialogDelegate,
      public WebDialogWebContentsDelegate {
 public:
  ConstrainedWebDialogDelegateBase(
      Profile* profile,
      WebDialogDelegate* delegate,
      WebDialogWebContentsDelegate* tab_delegate);
  virtual ~ConstrainedWebDialogDelegateBase();

  void set_window(ConstrainedWindow* window);
  bool closed_via_webui() const;

  // ConstrainedWebDialogDelegate interface.
  virtual const WebDialogDelegate*
      GetWebDialogDelegate() const OVERRIDE;
  virtual WebDialogDelegate* GetWebDialogDelegate() OVERRIDE;
  virtual void OnDialogCloseFromWebUI() OVERRIDE;
  virtual void ReleaseTabContentsOnDialogClose() OVERRIDE;
  virtual ConstrainedWindow* window() OVERRIDE;
  virtual TabContentsWrapper* tab() OVERRIDE;

  // WebDialogWebContentsDelegate interface.
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;

 protected:
  void set_override_tab_delegate(
      WebDialogWebContentsDelegate* override_tab_delegate);

 private:
  WebDialogDelegate* web_dialog_delegate_;

  // The constrained window that owns |this|. Saved so we can close it later.
  ConstrainedWindow* window_;

  // Holds the HTML to display in the constrained dialog.
  scoped_ptr<TabContentsWrapper> tab_;

  // Was the dialog closed from WebUI (in which case |web_dialog_delegate_|'s
  // OnDialogClosed() method has already been called)?
  bool closed_via_webui_;

  // If true, release |tab_| on close instead of destroying it.
  bool release_tab_on_close_;

  scoped_ptr<WebDialogWebContentsDelegate> override_tab_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateBase);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_DELEGATE_BASE_H_
