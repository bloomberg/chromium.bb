// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_DELEGATE_BASE_H_
#define CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_DELEGATE_BASE_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "ui/web_dialogs/constrained_web_dialog_ui.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

namespace ui {
class WebDialogDelegate;
}

// Platform-agnostic base implementation of ConstrainedWebDialogDelegate.
class ConstrainedWebDialogDelegateBase
    : public ui::ConstrainedWebDialogDelegate,
      public ui::WebDialogWebContentsDelegate {
 public:
  ConstrainedWebDialogDelegateBase(
      Profile* profile,
      ui::WebDialogDelegate* delegate,
      WebDialogWebContentsDelegate* tab_delegate);
  virtual ~ConstrainedWebDialogDelegateBase();

  void set_window(ConstrainedWindow* window);
  bool closed_via_webui() const;

  // ui::ConstrainedWebDialogDelegate interface.
  virtual const ui::WebDialogDelegate*
      GetWebDialogDelegate() const OVERRIDE;
  virtual ui::WebDialogDelegate* GetWebDialogDelegate() OVERRIDE;
  virtual void OnDialogCloseFromWebUI() OVERRIDE;
  virtual void ReleaseTabContentsOnDialogClose() OVERRIDE;
  virtual ConstrainedWindow* window() OVERRIDE;
  virtual TabContents* tab() OVERRIDE;

  // WebDialogWebContentsDelegate interface.
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE;

 protected:
  void set_override_tab_delegate(
      WebDialogWebContentsDelegate* override_tab_delegate);

 private:
  scoped_ptr<ui::WebDialogDelegate> web_dialog_delegate_;

  // The constrained window that owns |this|. Saved so we can close it later.
  ConstrainedWindow* window_;

  // Holds the HTML to display in the constrained dialog.
  scoped_ptr<TabContents> tab_;

  // Was the dialog closed from WebUI (in which case |web_dialog_delegate_|'s
  // OnDialogClosed() method has already been called)?
  bool closed_via_webui_;

  // If true, release |tab_| on close instead of destroying it.
  bool release_tab_on_close_;

  scoped_ptr<WebDialogWebContentsDelegate> override_tab_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateBase);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_DELEGATE_BASE_H_
