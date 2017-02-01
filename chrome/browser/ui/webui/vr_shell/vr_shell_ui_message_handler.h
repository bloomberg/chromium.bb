// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_VR_SHELL_VR_SHELL_UI_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_VR_SHELL_VR_SHELL_UI_MESSAGE_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/vr_shell/ui_interface.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace vr_shell {
class VrShell;
}

class VrShellUIMessageHandler : public content::WebUIMessageHandler,
                                public vr_shell::UiCommandHandler {
 public:
  VrShellUIMessageHandler();
  ~VrShellUIMessageHandler() override;

  void SendCommandToUi(const base::Value& value) override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;

  void HandleDomLoaded(const base::ListValue* args);
  void HandleUpdateScene(const base::ListValue* args);
  void HandleDoAction(const base::ListValue* args);
  void HandleSetContentCssSize(const base::ListValue* args);
  void HandleSetUiCssSize(const base::ListValue* args);
  void SetSize(const base::ListValue* args, bool for_ui);

  base::WeakPtr<vr_shell::VrShell> vr_shell_;

  DISALLOW_COPY_AND_ASSIGN(VrShellUIMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_VR_SHELL_VR_SHELL_UI_MESSAGE_HANDLER_H_
