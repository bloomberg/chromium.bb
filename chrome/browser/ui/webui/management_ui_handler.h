// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MANAGEMENT_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MANAGEMENT_UI_HANDLER_H_

#include "base/strings/string16.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

// The JavaScript message handler for the chrome://management page.
class ManagementUIHandler : public content::WebUIMessageHandler {
 public:
  ManagementUIHandler();
  ~ManagementUIHandler() override;

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void HandleInitialized(const base::ListValue* args);

  void ShowDeviceManagementStatus();

  DISALLOW_COPY_AND_ASSIGN(ManagementUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MANAGEMENT_UI_HANDLER_H_
