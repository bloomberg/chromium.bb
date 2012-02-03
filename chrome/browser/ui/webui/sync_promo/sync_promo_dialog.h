// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_PROMO_SYNC_PROMO_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_PROMO_SYNC_PROMO_DIALOG_H_
#pragma once

#include "chrome/browser/ui/webui/html_dialog_ui.h"

class Profile;
class Browser;

// Shows the sync promo in an HTML dialog.
class SyncPromoDialog : public HtmlDialogUIDelegate {
 public:
  // |profile| is the profile that should own the HTML dialog. |url| is the
  // URL to display inside the dialog.
  SyncPromoDialog(Profile* profile, GURL url);
  virtual ~SyncPromoDialog();

  // Shows the dialog and blocks until the dialog is dismissed.
  void ShowDialog();

  // Returns the browser spawned from the sync promo dialog (if any).
  Browser* spawned_browser() const { return spawned_browser_; }

  // Returns true if the sync promo was closed.
  bool sync_promo_was_closed() const { return sync_promo_was_closed_; }

  // Returns the dialog window.
  gfx::NativeWindow window() const { return window_; }

 private:
  // HtmlDialogUIDelegate:
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
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;
  virtual bool HandleOpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      content::WebContents** out_new_contents) OVERRIDE;
  virtual bool HandleAddNewContents(content::WebContents* source,
                                    content::WebContents* new_contents,
                                    WindowOpenDisposition disposition,
                                    const gfx::Rect& initial_pos,
                                    bool user_gesture) OVERRIDE;

  // The profile that should own the HTML dialog.
  Profile* profile_;
  // The browser spawned from the sync promo dialog (if any).
  Browser* spawned_browser_;
  // This is set to true if the sync promo was closed.
  bool sync_promo_was_closed_;
  // The URL to dispaly in the HTML dialog.
  GURL url_;
  // The dialog window.
  gfx::NativeWindow window_;

  DISALLOW_COPY_AND_ASSIGN(SyncPromoDialog);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_PROMO_SYNC_PROMO_DIALOG_H_
