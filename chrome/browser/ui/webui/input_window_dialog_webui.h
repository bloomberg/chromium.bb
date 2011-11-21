// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INPUT_WINDOW_DIALOG_WEBUI_H_
#define CHROME_BROWSER_UI_WEBUI_INPUT_WINDOW_DIALOG_WEBUI_H_
#pragma once

#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/ui/input_window_dialog.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"

namespace base {
class ListValue;
}  // namespace base

class InputWindowDialogHandler;

// A class that implements InputWindowDialog methods with WebUI.
class InputWindowDialogWebUI : public InputWindowDialog,
                               private HtmlDialogUIDelegate {
 public:
  InputWindowDialogWebUI(const string16& window_title,
                         const LabelContentsPairs& label_contents_pairs,
                         InputWindowDialog::Delegate* delegate,
                         ButtonType type);
  virtual ~InputWindowDialogWebUI();

  // InputWindowDialog methods
  virtual void Show() OVERRIDE;
  virtual void Close() OVERRIDE;

 private:
  // HtmlDialogUIDelegate methods
  virtual bool IsDialogModal() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(TabContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;

  // The dialog handler.
  InputWindowDialogHandler* handler_;

  string16 window_title_;
  LabelContentsPairs label_contents_pairs_;
  bool closed_;

  Delegate* delegate_;
  ButtonType type_;

  DISALLOW_COPY_AND_ASSIGN(InputWindowDialogWebUI);
};

// Dialog handler that handles calls from the JS WebUI code to validate the
// string value in the text field.
class InputWindowDialogHandler : public WebUIMessageHandler {
 public:
  explicit InputWindowDialogHandler(InputWindowDialog::Delegate* delegate);

  void CloseDialog();

  // Overridden from WebUIMessageHandler
  virtual void RegisterMessages() OVERRIDE;

 private:
  void Validate(const base::ListValue* args);

  InputWindowDialog::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(InputWindowDialogHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INPUT_WINDOW_DIALOG_WEBUI_H_
