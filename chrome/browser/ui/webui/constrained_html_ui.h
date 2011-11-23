// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONSTRAINED_HTML_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CONSTRAINED_HTML_UI_H_
#pragma once

#include "chrome/browser/ui/webui/chrome_web_ui.h"

class ConstrainedWindow;
class HtmlDialogUIDelegate;
class Profile;
class RenderViewHost;
class TabContents;
class TabContentsWrapper;

namespace base {
template<class T> class PropertyAccessor;
}

class ConstrainedHtmlUIDelegate {
 public:
  virtual HtmlDialogUIDelegate* GetHtmlDialogUIDelegate() = 0;

  // Called when the dialog is being closed in response to a "DialogClose"
  // message from WebUI.
  virtual void OnDialogCloseFromWebUI() = 0;

  // If called, on dialog closure, the dialog will release its TabContents
  // instead of destroying it. After which point, the caller will own the
  // released TabContents.
  virtual void ReleaseTabContentsOnDialogClose() = 0;

  // Returns the ConstrainedWindow.
  virtual ConstrainedWindow* window() = 0;

  // Returns the TabContentsWrapper owned by the constrained window.
  virtual TabContentsWrapper* tab() = 0;

 protected:
  virtual ~ConstrainedHtmlUIDelegate() {}
};

// ConstrainedHtmlUI is a facility to show HTML WebUI content
// in a tab-modal constrained dialog.  It is implemented as an adapter
// between an HtmlDialogUI object and a ConstrainedWindow object.
//
// Since ConstrainedWindow requires platform-specific delegate
// implementations, this class is just a factory stub.
class ConstrainedHtmlUI : public ChromeWebUI {
 public:
  explicit ConstrainedHtmlUI(TabContents* contents);
  virtual ~ConstrainedHtmlUI();

  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;

  // Create a constrained HTML dialog. The actual object that gets created
  // is a ConstrainedHtmlUIDelegate, which later triggers construction of a
  // ConstrainedHtmlUI object.
  static ConstrainedHtmlUIDelegate* CreateConstrainedHtmlDialog(
      Profile* profile,
      HtmlDialogUIDelegate* delegate,
      TabContentsWrapper* overshadowed);

  // Returns a property accessor that can be used to set the
  // ConstrainedHtmlUIDelegate property on a TabContents.
  static base::PropertyAccessor<ConstrainedHtmlUIDelegate*>&
      GetPropertyAccessor();

 protected:
  // Returns the TabContents' PropertyBag's ConstrainedHtmlUIDelegate.
  // Returns NULL if that property is not set.
  ConstrainedHtmlUIDelegate* GetConstrainedDelegate();

 private:
  // JS Message Handler
  void OnDialogCloseMessage(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(ConstrainedHtmlUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONSTRAINED_HTML_UI_H_
