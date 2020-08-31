// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_DIALOG_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

namespace content {
class WebContents;
class WebUIMessageHandler;
}  // namespace content

namespace gfx {
class Size;
}  // namespace gfx

namespace chromeos {

namespace login_screen_extension_ui {

struct CreateOptions;

// This class is used to provide data from a chrome.loginScreenUi API call to
// the WebDialog.
class DialogDelegate : public ui::WebDialogDelegate {
 public:
  explicit DialogDelegate(CreateOptions* create_options);
  ~DialogDelegate() override;

  void set_can_close(bool can_close) { can_close_ = can_close; }
  void set_native_window(gfx::NativeWindow native_window) {
    native_window_ = native_window;
  }

  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  base::string16 GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  bool CanCloseDialog() const override;
  bool CanResizeDialog() const override;
  void GetDialogSize(gfx::Size* size) const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  std::string GetDialogArgs() const override;
  // NOTE: This function deletes this object at the end.
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldCloseDialogOnEscape() const override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldCenterDialogTitleText() const override;

 private:
  const std::string extension_name_;
  const GURL content_url_;
  bool can_close_ = false;

  base::OnceClosure close_callback_;

  gfx::NativeWindow native_window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DialogDelegate);
};

}  // namespace login_screen_extension_ui

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_DIALOG_DELEGATE_H_
