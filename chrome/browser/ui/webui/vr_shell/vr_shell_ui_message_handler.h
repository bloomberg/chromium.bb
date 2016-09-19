// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_VR_SHELL_VR_SHELL_UI_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_VR_SHELL_VR_SHELL_UI_MESSAGE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace vr_shell {
class VrShell;
}

class VrShellUIMessageHandler : public content::WebUIMessageHandler {
 public:
  VrShellUIMessageHandler();
  ~VrShellUIMessageHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  void HandleDomLoaded(const base::ListValue* args);
  void HandleSetDevicePixelRatio(const base::ListValue* args);
  void HandleAddMesh(const base::ListValue* args);
  void HandleRemoveMesh(const base::ListValue* args);
  void HandleAddAnimations(const base::ListValue* args);
  void HandleDoAction(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(VrShellUIMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_VR_SHELL_VR_SHELL_UI_MESSAGE_HANDLER_H_
