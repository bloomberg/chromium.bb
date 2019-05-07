// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UI_H_

#include <vector>

#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_utils.h"
#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// Controller for chrome://assistant-optin/ page.
class AssistantOptInUI : public ui::WebDialogUI {
 public:
  explicit AssistantOptInUI(content::WebUI* web_ui);
  ~AssistantOptInUI() override;

  // Called when the dialog is closed.
  void OnDialogClosed();

 private:
  // Called when the webui has been initialized.
  void Initialize();

  JSCallsContainer js_calls_container_;
  AssistantOptInFlowScreenHandler* assistant_handler_ptr_;
  base::WeakPtrFactory<AssistantOptInUI> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantOptInUI);
};

// Dialog delegate for the assistant optin page.
class AssistantOptInDialog : public SystemWebDialogDelegate {
 public:
  // Shows the assistant optin dialog.
  static void Show(
      ash::mojom::FlowType type = ash::mojom::FlowType::CONSENT_FLOW,
      ash::mojom::AssistantSetup::StartAssistantOptInFlowCallback callback =
          base::DoNothing());

  // Returns whether the dialog is being shown.
  static bool IsActive();

 protected:
  AssistantOptInDialog(
      ash::mojom::FlowType type,
      ash::mojom::AssistantSetup::StartAssistantOptInFlowCallback callback);
  ~AssistantOptInDialog() override;

  // SystemWebDialogDelegate
  views::Widget::InitParams GetInitParams() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  bool ShouldShowDialogTitle() const override;
  void OnDialogShown(content::WebUI* webui,
                     content::RenderViewHost* render_view_host) override;
  void OnDialogClosed(const std::string& json_retval) override;
  bool CanCloseDialog() const override;

 private:
  AssistantOptInUI* assistant_ui_ = nullptr;

  // Callback to run if the flow is completed.
  ash::mojom::AssistantSetup::StartAssistantOptInFlowCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(AssistantOptInDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UI_H_
