// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_CONSTRAINED_HTML_DIALOG_H_
#define CHROME_BROWSER_DOM_UI_CONSTRAINED_HTML_DIALOG_H_
#pragma once

#include <vector>

#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/tab_contents/constrained_window.h"

class HtmlDialogUIDelegate;
class Profile;
class RenderViewHost;

// ConstrainedHtmlDialog is a facility to show HTML DOM_UI content
// in a tab-modal constrained dialog.  It is implemented as an adapter
// between an HtmlDialogUI object and a ConstrainedWindow object.
//
// Since ConstrainedWindow requires platform-specific delegate
// implementations, this class is just a factory stub.
class ConstrainedHtmlDialog : public DOMUI {
 public:
  static ConstrainedHtmlDialog* CreateConstrainedHTMLDialog(
      Profile* profile,
      HtmlDialogUIDelegate* delegate);

  virtual ConstrainedWindowDelegate* GetConstrainedWindowDelegate() = 0;

  // DOMUI override since this class does not use TabContents.
  Profile* GetProfile() const { return profile_; }

 protected:
  ConstrainedHtmlDialog(Profile* profile,
                        HtmlDialogUIDelegate* delegate);
  virtual ~ConstrainedHtmlDialog();

  HtmlDialogUIDelegate* html_dialog_ui_delegate() {
    return html_dialog_ui_delegate_;
  }

  // DOMUI override since this class does not use TabContents.
  RenderViewHost* GetRenderViewHost() const { return render_view_host_; }

  void InitializeDOMUI(RenderViewHost* render_view_host);

 private:
  // JS Message Handler
  void OnDialogClosed(const ListValue* args);

  Profile* profile_;
  RenderViewHost* render_view_host_;
  HtmlDialogUIDelegate* html_dialog_ui_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedHtmlDialog);
};

#endif  // CHROME_BROWSER_DOM_UI_CONSTRAINED_HTML_DIALOG_H_
