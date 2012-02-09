// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONSTRAINED_HTML_UI_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_CONSTRAINED_HTML_UI_DELEGATE_IMPL_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/webui/constrained_html_ui.h"
#include "chrome/browser/ui/webui/html_dialog_tab_contents_delegate.h"

// Platform-agnostic base implementation of ConstrainedHtmlUIDelegate.
class ConstrainedHtmlUIDelegateImpl : public ConstrainedHtmlUIDelegate,
                                      public HtmlDialogTabContentsDelegate {
 public:
  ConstrainedHtmlUIDelegateImpl(Profile* profile,
                                HtmlDialogUIDelegate* delegate,
                                HtmlDialogTabContentsDelegate* tab_delegate);
  virtual ~ConstrainedHtmlUIDelegateImpl();

  void set_window(ConstrainedWindow* window);
  bool closed_via_webui() const;

  // ConstrainedHtmlUIDelegate interface.
  virtual const HtmlDialogUIDelegate* GetHtmlDialogUIDelegate() const OVERRIDE;
  virtual HtmlDialogUIDelegate* GetHtmlDialogUIDelegate() OVERRIDE;
  virtual void OnDialogCloseFromWebUI() OVERRIDE;
  virtual void ReleaseTabContentsOnDialogClose() OVERRIDE;
  virtual ConstrainedWindow* window() OVERRIDE;
  virtual TabContentsWrapper* tab() OVERRIDE;

  // HtmlDialogTabContentsDelegate interface.
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;

 protected:
  void set_override_tab_delegate(
      HtmlDialogTabContentsDelegate* override_tab_delegate);

 private:
  HtmlDialogUIDelegate* html_delegate_;

  // The constrained window that owns |this|. Saved so we can close it later.
  ConstrainedWindow* window_;

  // Holds the HTML to display in the constrained dialog.
  scoped_ptr<TabContentsWrapper> tab_;

  // Was the dialog closed from WebUI (in which case |html_delegate_|'s
  // OnDialogClosed() method has already been called)?
  bool closed_via_webui_;

  // If true, release |tab_| on close instead of destroying it.
  bool release_tab_on_close_;

  scoped_ptr<HtmlDialogTabContentsDelegate> override_tab_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedHtmlUIDelegateImpl);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONSTRAINED_HTML_UI_DELEGATE_IMPL_H_
