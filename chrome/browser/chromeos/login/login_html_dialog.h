// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_HTML_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_HTML_DIALOG_H_
#pragma once

#include <string>

#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "gfx/native_widget_types.h"
#include "gfx/size.h"

namespace chromeos {

// Launches html dialog during OOBE/Login with specified URL and title.
class LoginHtmlDialog : public HtmlDialogUIDelegate {
 public:
  // Delegate class to get notifications from the dialog.
  class Delegate {
  public:
    virtual ~Delegate() {}

    // Called when dialog has been closed.
    virtual void OnDialogClosed() = 0;
  };

  LoginHtmlDialog(Delegate* delegate,
                  gfx::NativeWindow parent_window,
                  const std::wstring& title,
                  const GURL& url);
  ~LoginHtmlDialog();

  // Shows created dialog.
  void Show();

  // Overrides default width/height for dialog.
  void SetDialogSize(int width, int height);

 protected:
  // HtmlDialogUIDelegate implementation.
  virtual bool IsDialogModal() const { return true; }
  virtual std::wstring GetDialogTitle() const { return title_; }
  virtual GURL GetDialogContentURL() const { return url_; }
  virtual void GetDOMMessageHandlers(
      std::vector<DOMMessageHandler*>* handlers) const {}
  virtual void GetDialogSize(gfx::Size* size) const;
  virtual std::string GetDialogArgs() const { return std::string(); }
  virtual void OnDialogClosed(const std::string& json_retval);
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog);

 private:
  // Notifications receiver.
  Delegate* delegate_;

  gfx::NativeWindow parent_window_;
  std::wstring title_;
  GURL url_;

  // Dialog display size.
  int width_;
  int height_;

  DISALLOW_COPY_AND_ASSIGN(LoginHtmlDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_HTML_DIALOG_H_
