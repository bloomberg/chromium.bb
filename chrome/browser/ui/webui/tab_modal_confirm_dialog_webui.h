// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_MODAL_CONFIRM_DIALOG_WEBUI_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_MODAL_CONFIRM_DIALOG_WEBUI_H_
#pragma once

#if !(defined(USE_AURA) || defined(OS_CHROMEOS))
#error Tab-modal confirm dialog should be shown with native UI.
#endif

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class TabContents;
class TabModalConfirmDialogDelegate;

namespace ui {
class ConstrainedWebDialogDelegate;
}

// Displays a tab-modal dialog, i.e. a dialog that will block the current page
// but still allow the user to switch to a different page.
// To display the dialog, allocate this object on the heap. It will open the
// dialog from its constructor and then delete itself when the user dismisses
// the dialog.
class TabModalConfirmDialogWebUI : public ui::WebDialogDelegate {
 public:
  TabModalConfirmDialogWebUI(
      TabModalConfirmDialogDelegate* dialog_delegate,
      TabContents* tab_contents);

  // ui::WebDialogDelegate implementation.
  virtual ui::ModalType GetDialogModalType() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(content::WebContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;

  ui::ConstrainedWebDialogDelegate* constrained_web_dialog_delegate() {
    return constrained_web_dialog_delegate_;
  }

 private:
  virtual ~TabModalConfirmDialogWebUI();

  scoped_ptr<TabModalConfirmDialogDelegate> delegate_;

  // Deletes itself.
  ui::ConstrainedWebDialogDelegate* constrained_web_dialog_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TabModalConfirmDialogWebUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_MODAL_CONFIRM_DIALOG_WEBUI_H_
