// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CLEANUP_TOOL_CLEANUP_ACTION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CLEANUP_TOOL_CLEANUP_ACTION_HANDLER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/web_ui_message_handler.h"

// The handler for Javascript messages related to the "Chrome Cleanup" view.
class CleanupActionHandler : public content::WebUIMessageHandler {
 public:
  CleanupActionHandler();
  ~CleanupActionHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void HandleRequestLastScanResult(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(CleanupActionHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CLEANUP_TOOL_CLEANUP_ACTION_HANDLER_H_
