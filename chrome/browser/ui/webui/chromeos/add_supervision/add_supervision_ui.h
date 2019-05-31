// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_UI_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision.mojom.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ui/base/ui_base_types.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// Dialog which displays the add-supeivision flow which allows users to
// convert a regular Google account into a Family-Link managed account.
class AddSupervisionDialog : public SystemWebDialogDelegate {
 public:
  // Shows the dialog; if the dialog is already displayed, this function is a
  // no-op.
  static void Show();

  // Registers a callback which will be called when the dialog is closed.
  void AddOnCloseCallback(base::OnceClosure callback);

  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  void GetDialogSize(gfx::Size* size) const override;
  void OnDialogClosed(const std::string& json_retval) override;

 protected:
  AddSupervisionDialog();
  ~AddSupervisionDialog() override;

 private:
  static SystemWebDialogDelegate* GetInstance();

  // List of callbacks that have registered themselves to be invoked once this
  // dialog is closed.
  std::vector<base::OnceClosure> on_close_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(AddSupervisionDialog);
};

// Controller for chrome://add-supervision
class AddSupervisionUI : public ui::MojoWebDialogUI {
 public:
  explicit AddSupervisionUI(content::WebUI* web_ui);
  ~AddSupervisionUI() override;

 private:
  void BindAddSupervisionHandler(
      add_supervision::mojom::AddSupervisionHandlerRequest request);
  void SetupResources();

  std::unique_ptr<add_supervision::mojom::AddSupervisionHandler>
      mojo_api_handler_;

  DISALLOW_COPY_AND_ASSIGN(AddSupervisionUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_UI_H_
