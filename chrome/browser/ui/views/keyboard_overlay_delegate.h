// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_KEYBOARD_OVERLAY_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_KEYBOARD_OVERLAY_DELEGATE_H_

#include "chrome/browser/ui/webui/html_dialog_ui.h"

class HtmlDialogView;

class KeyboardOverlayDelegate : public HtmlDialogUIDelegate {
 public:
  explicit KeyboardOverlayDelegate(const string16& title);

  HtmlDialogView* view() { return view_; }
  void set_view(HtmlDialogView* html_view) { view_ = html_view; }

  // Overridden from HtmlDialogUIDelegate:
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;

 private:
  virtual ~KeyboardOverlayDelegate();

  // Overridden from HtmlDialogUIDelegate:
  virtual ui::ModalType GetDialogModalType() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(content::WebContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;

  // The dialog title.
  string16 title_;

  // The view associated with this delegate.
  // This class does not own the pointer.
  HtmlDialogView* view_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardOverlayDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_KEYBOARD_OVERLAY_DELEGATE_H_
