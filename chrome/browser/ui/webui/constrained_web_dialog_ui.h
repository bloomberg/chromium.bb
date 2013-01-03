// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_UI_H_

#include "base/compiler_specific.h"
#include "content/public/browser/web_ui_controller.h"

class WebContentsModalDialog;

namespace content {
class BrowserContext;
class RenderViewHost;
class WebContents;
}

namespace ui {
class WebDialogDelegate;
class WebDialogWebContentsDelegate;
}

class ConstrainedWebDialogDelegate {
 public:
  virtual const ui::WebDialogDelegate* GetWebDialogDelegate() const = 0;
  virtual ui::WebDialogDelegate* GetWebDialogDelegate() = 0;

  // Called when the dialog is being closed in response to a "DialogClose"
  // message from WebUI.
  virtual void OnDialogCloseFromWebUI() = 0;

  // If called, on dialog closure, the dialog will release its WebContents
  // instead of destroying it. After which point, the caller will own the
  // released WebContents.
  virtual void ReleaseWebContentsOnDialogClose() = 0;

  // Returns the WebContentsModalDialog.
  virtual WebContentsModalDialog* GetWindow() = 0;

  // Returns the WebContents owned by the constrained window.
  virtual content::WebContents* GetWebContents() = 0;

 protected:
  virtual ~ConstrainedWebDialogDelegate() {}
};

// ConstrainedWebDialogUI is a facility to show HTML WebUI content
// in a tab-modal constrained dialog.  It is implemented as an adapter
// between an WebDialogUI object and a WebContentsModalDialog object.
//
// Since WebContentsModalDialog requires platform-specific delegate
// implementations, this class is just a factory stub.
// TODO(thestig): Refactor the platform-independent code out of the
// platform-specific implementations.
class ConstrainedWebDialogUI
    : public content::WebUIController {
 public:
  explicit ConstrainedWebDialogUI(content::WebUI* web_ui);
  virtual ~ConstrainedWebDialogUI();

  // WebUIController implementation:
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;

  // Sets the delegate on the WebContents.
  static void SetConstrainedDelegate(content::WebContents* web_contents,
                                     ConstrainedWebDialogDelegate* delegate);

 protected:
  // Returns the ConstrainedWebDialogDelegate saved with the WebContents.
  // Returns NULL if no such delegate is set.
  ConstrainedWebDialogDelegate* GetConstrainedDelegate();

 private:
  // JS Message Handler
  void OnDialogCloseMessage(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogUI);
};

// Create a constrained HTML dialog. The actual object that gets created
// is a ConstrainedWebDialogDelegate, which later triggers construction of a
// ConstrainedWebDialogUI object.
// |browser_context| is used to construct the constrained HTML dialog's
//                   WebContents.
// |delegate| controls the behavior of the dialog.
// |tab_delegate| is optional, pass one in to use a custom
//                WebDialogWebContentsDelegate with the dialog, or NULL to
//                use the default one. The dialog takes ownership of
//                |tab_delegate|.
// |overshadowed| is the tab being overshadowed by the dialog.
ConstrainedWebDialogDelegate* CreateConstrainedWebDialog(
    content::BrowserContext* browser_context,
    ui::WebDialogDelegate* delegate,
    ui::WebDialogWebContentsDelegate* tab_delegate,
    content::WebContents* overshadowed);

#endif  // CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_UI_H_
