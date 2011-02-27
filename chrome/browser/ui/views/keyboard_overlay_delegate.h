// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_KEYBOARD_OVERLAY_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_KEYBOARD_OVERLAY_DELEGATE_H_

#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "ui/gfx/native_widget_types.h"

class KeyboardOverlayDelegate : public HtmlDialogUIDelegate {
 public:
  explicit KeyboardOverlayDelegate(const std::wstring& title);

  // Shows the keyboard overlay dialog box.
  static void ShowDialog(gfx::NativeWindow owning_window);

 private:
  ~KeyboardOverlayDelegate();

  // Overridden from HtmlDialogUI::Delegate:
  virtual bool IsDialogModal() const;
  virtual std::wstring GetDialogTitle() const;
  virtual GURL GetDialogContentURL() const;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const;
  virtual void GetDialogSize(gfx::Size* size) const;
  virtual std::string GetDialogArgs() const;
  virtual void OnDialogClosed(const std::string& json_retval);
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog);
  virtual bool ShouldShowDialogTitle() const;

  // The dialog title.
  std::wstring title_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardOverlayDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_KEYBOARD_OVERLAY_DELEGATE_H_
