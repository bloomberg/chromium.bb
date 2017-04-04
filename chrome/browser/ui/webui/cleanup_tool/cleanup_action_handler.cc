// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cleanup_tool/cleanup_action_handler.h"

#include "base/bind.h"
#include "base/values.h"

CleanupActionHandler::CleanupActionHandler() {}

CleanupActionHandler::~CleanupActionHandler() {}

void CleanupActionHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestLastScanResult",
      base::Bind(&CleanupActionHandler::HandleRequestLastScanResult,
                 base::Unretained(this)));
}

void CleanupActionHandler::HandleRequestLastScanResult(
    const base::ListValue* args) {
  std::string webui_callback_id;
  CHECK_EQ(1U, args->GetSize());
  bool success = args->GetString(0, &webui_callback_id);
  DCHECK(success);

  base::DictionaryValue last_scan_results;
  // TODO(proberge): Return real information about the last run.
  // TODO(proberge): Localize strings once they are finalized.
  last_scan_results.SetBoolean("hasScanResults", false);
  last_scan_results.SetBoolean("isInfected", false);
  last_scan_results.SetString("detectionStatusText", "No problems detected");
  last_scan_results.SetString("detectionTimeText", "Last scanned today");

  AllowJavascript();
  ResolveJavascriptCallback(base::Value(webui_callback_id), last_scan_results);
}
