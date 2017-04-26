// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cleanup_tool/cleanup_action_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/sw_reporter_installer_win.h"
#include "components/component_updater/component_updater_service.h"

namespace {

bool IsCleanupComponentRegistered() {
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  std::vector<std::string> component_ids = cus->GetComponentIDs();

  return std::find(component_ids.begin(), component_ids.end(),
                   component_updater::kSwReporterComponentId) !=
         component_ids.end();
}

// TODO(proberge): Connect this to the Cleanup Tool manager class once that's
// implemented.
void StartScan(base::Closure callback) {
  callback.Run();
}

// TODO(proberge): Connect this to the Cleanup Tool manager class once that's
// implemented.
void StartCleanup(base::Closure callback) {
  callback.Run();
}

// TODO(proberge): Localize strings once they are finalized.
constexpr char kDetectionOkText[] = "No problems detected";
constexpr char kDetectionUwSText[] = "2 potentially harmful programs detected";
constexpr char kDetectionTimeText[] = "Last scanned today";
constexpr char kCleanupUnsupportedText[] = "Chrome Cleanup is not supported.";
constexpr char kUnsupportedHelpText[] = "Please try reinstalling Chrome";

}  // namespace

CleanupActionHandler::CleanupActionHandler()
    : callback_weak_ptr_factory_(this) {}

CleanupActionHandler::~CleanupActionHandler() {}

void CleanupActionHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestLastScanResult",
      base::Bind(&CleanupActionHandler::HandleRequestLastScanResult,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startScan", base::Bind(&CleanupActionHandler::HandleStartScan,
                              base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startCleanup", base::Bind(&CleanupActionHandler::HandleStartCleanup,
                                 base::Unretained(this)));
}

void CleanupActionHandler::OnJavascriptDisallowed() {
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
}

void CleanupActionHandler::HandleRequestLastScanResult(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string webui_callback_id;
  bool success = args->GetString(0, &webui_callback_id);
  DCHECK(success);

  base::DictionaryValue last_scan_results;
  // TODO(proberge): Return real information about the last run.
  last_scan_results.SetBoolean("hasScanResults", false);
  last_scan_results.SetBoolean("isInfected", false);
  last_scan_results.SetString("detectionStatusText", kDetectionOkText);
  last_scan_results.SetString("detectionTimeText", kDetectionTimeText);

  AllowJavascript();
  ResolveJavascriptCallback(base::Value(webui_callback_id), last_scan_results);
}

void CleanupActionHandler::HandleStartScan(const base::ListValue* args) {
  std::string webui_callback_id;
  CHECK_EQ(1U, args->GetSize());
  bool success = args->GetString(0, &webui_callback_id);
  DCHECK(success);

  if (!IsCleanupComponentRegistered()) {
    base::DictionaryValue scan_results;
    scan_results.SetBoolean("hasScanResults", false);
    scan_results.SetBoolean("isInfected", false);
    scan_results.SetString("detectionStatusText", kCleanupUnsupportedText);
    scan_results.SetString("detectionTimeText", kUnsupportedHelpText);

    AllowJavascript();
    ResolveJavascriptCallback(base::Value(webui_callback_id), scan_results);
    return;
  }

  StartScan(base::Bind(&CleanupActionHandler::ReportScanResults,
                       callback_weak_ptr_factory_.GetWeakPtr(),
                       webui_callback_id));
}

void CleanupActionHandler::ReportScanResults(const std::string& callback_id) {
  base::DictionaryValue scan_results;
  // TODO(proberge): Return real information about the scan.
  scan_results.SetBoolean("hasScanResults", true);
  scan_results.SetBoolean("isInfected", true);
  scan_results.SetString("detectionStatusText", kDetectionUwSText);
  scan_results.SetString("detectionTimeText", kDetectionTimeText);

  AllowJavascript();
  ResolveJavascriptCallback(base::Value(callback_id), scan_results);
}

void CleanupActionHandler::HandleStartCleanup(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string webui_callback_id;
  bool success = args->GetString(0, &webui_callback_id);
  DCHECK(success);

  StartCleanup(base::Bind(&CleanupActionHandler::ReportCleanupResults,
                          base::Unretained(this), webui_callback_id));
}

void CleanupActionHandler::ReportCleanupResults(
    const std::string& callback_id) {
  base::DictionaryValue cleanup_results;
  // TODO(proberge): Return real information about the cleanup.
  cleanup_results.SetBoolean("wasCancelled", true);
  cleanup_results.SetBoolean("requiresReboot", false);

  ResolveJavascriptCallback(base::Value(callback_id), cleanup_results);
}
