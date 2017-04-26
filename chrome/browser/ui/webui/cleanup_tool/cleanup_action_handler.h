// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CLEANUP_TOOL_CLEANUP_ACTION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CLEANUP_TOOL_CLEANUP_ACTION_HANDLER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_message_handler.h"

// The handler for Javascript messages related to the "Chrome Cleanup" view.
class CleanupActionHandler : public content::WebUIMessageHandler {
 public:
  CleanupActionHandler();
  ~CleanupActionHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Invalidates the weak pointers in callbacks that are no longer safe to run.
  void OnJavascriptDisallowed() override;

  void HandleRequestLastScanResult(const base::ListValue* args);
  void HandleStartScan(const base::ListValue* args);
  void HandleStartCleanup(const base::ListValue* args);

  // Returns the scan result initiated by HandleStartScan() to the Javascript
  // caller refererenced by |callback_id|.
  void ReportScanResults(const std::string& callback_id);

  // Returns the cleanup result initiated by HandleStartCleanup() to the
  // Javascript caller refererenced by |callback_id|.
  void ReportCleanupResults(const std::string& callback_id);

  // Used to cancel callbacks when JavaScript becomes disallowed.
  base::WeakPtrFactory<CleanupActionHandler> callback_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CleanupActionHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CLEANUP_TOOL_CLEANUP_ACTION_HANDLER_H_
