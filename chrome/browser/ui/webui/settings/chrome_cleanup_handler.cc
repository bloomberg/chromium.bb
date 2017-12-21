// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chrome_cleanup_handler.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/sw_reporter_installer_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"

using safe_browsing::ChromeCleanerController;

namespace settings {

namespace {

// These numeric values must be kept in sync with the definition of
// settings.ChromeCleanupDismissSource in
// chrome/browser/resources/settings/chrome_cleanup_page/chrome_cleanup_page.js.
enum ChromeCleanerDismissSource {
  kOther = 0,
  kCleanupSuccessDoneButton = 1,
  kCleanupFailureDoneButton = 2,
};

// Returns a ListValue containing a copy of the file paths stored in |files|.
base::ListValue GetFilesAsListStorage(const std::set<base::FilePath>& files) {
  base::ListValue value;
  for (const base::FilePath& path : files)
    value.AppendString(path.value());

  return value;
}

std::string IdleReasonToString(
    ChromeCleanerController::IdleReason idle_reason) {
  switch (idle_reason) {
    case ChromeCleanerController::IdleReason::kInitial:
      return "initial";
    case ChromeCleanerController::IdleReason::kReporterFoundNothing:
      return "reporter_found_nothing";
    case ChromeCleanerController::IdleReason::kReporterFailed:
      return "reporter_failed";
    case ChromeCleanerController::IdleReason::kScanningFoundNothing:
      return "scanning_found_nothing";
    case ChromeCleanerController::IdleReason::kScanningFailed:
      return "scanning_failed";
    case ChromeCleanerController::IdleReason::kConnectionLost:
      return "connection_lost";
    case ChromeCleanerController::IdleReason::kUserDeclinedCleanup:
      return "user_declined_cleanup";
    case ChromeCleanerController::IdleReason::kCleaningFailed:
      return "cleaning_failed";
    case ChromeCleanerController::IdleReason::kCleaningSucceeded:
      return "cleaning_succeeded";
  }
  NOTREACHED();
  return "";
}

}  // namespace

ChromeCleanupHandler::ChromeCleanupHandler(Profile* profile)
    : controller_(ChromeCleanerController::GetInstance()), profile_(profile) {}

ChromeCleanupHandler::~ChromeCleanupHandler() {
  controller_->RemoveObserver(this);
}

void ChromeCleanupHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "dismissCleanupPage",
      base::BindRepeating(&ChromeCleanupHandler::HandleDismiss,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "registerChromeCleanerObserver",
      base::BindRepeating(
          &ChromeCleanupHandler::HandleRegisterChromeCleanerObserver,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startScanning",
      base::BindRepeating(&ChromeCleanupHandler::HandleStartScanning,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "restartComputer",
      base::BindRepeating(&ChromeCleanupHandler::HandleRestartComputer,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setLogsUploadPermission",
      base::BindRepeating(&ChromeCleanupHandler::HandleSetLogsUploadPermission,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startCleanup",
      base::BindRepeating(&ChromeCleanupHandler::HandleStartCleanup,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "notifyShowDetails",
      base::BindRepeating(&ChromeCleanupHandler::HandleNotifyShowDetails,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "notifyChromeCleanupLearnMoreClicked",
      base::BindRepeating(
          &ChromeCleanupHandler::HandleNotifyChromeCleanupLearnMoreClicked,
          base::Unretained(this)));
}

void ChromeCleanupHandler::OnJavascriptAllowed() {
  controller_->AddObserver(this);
}

void ChromeCleanupHandler::OnJavascriptDisallowed() {
  controller_->RemoveObserver(this);
}

void ChromeCleanupHandler::OnIdle(
    ChromeCleanerController::IdleReason idle_reason) {
  FireWebUIListener("chrome-cleanup-on-idle",
                    base::Value(IdleReasonToString(idle_reason)));
}

void ChromeCleanupHandler::OnScanning() {
  FireWebUIListener("chrome-cleanup-on-scanning");
}

void ChromeCleanupHandler::OnReporterRunning() {
  FireWebUIListener("chrome-cleanup-on-reporter-running");
}

void ChromeCleanupHandler::OnInfected(
    const safe_browsing::ChromeCleanerScannerResults& scanner_results) {
  FireWebUIListener("chrome-cleanup-on-infected",
                    GetFilesAsListStorage(scanner_results.files_to_delete()));
}

void ChromeCleanupHandler::OnCleaning(
    const safe_browsing::ChromeCleanerScannerResults& scanner_results) {
  FireWebUIListener("chrome-cleanup-on-cleaning",
                    GetFilesAsListStorage(scanner_results.files_to_delete()));
}

void ChromeCleanupHandler::OnRebootRequired() {
  FireWebUIListener("chrome-cleanup-on-reboot-required");
}

void ChromeCleanupHandler::OnLogsEnabledChanged(bool logs_enabled) {
  FireWebUIListener("chrome-cleanup-upload-permission-change",
                    base::Value(logs_enabled));
}

void ChromeCleanupHandler::HandleDismiss(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  int dismiss_source_int = -1;
  CHECK(args->GetInteger(0, &dismiss_source_int));

  ChromeCleanerDismissSource dismiss_source =
      static_cast<ChromeCleanerDismissSource>(dismiss_source_int);

  switch (dismiss_source) {
    case kCleanupSuccessDoneButton:
      base::RecordAction(base::UserMetricsAction(
          "SoftwareReporter.CleanupWebui_CleanupSuccessDone"));
      break;
    case kCleanupFailureDoneButton:
      base::RecordAction(base::UserMetricsAction(
          "SoftwareReporter.CleanupWebui_CleanupFailureDone"));
      break;
    case kOther:
      break;
    default:
      NOTREACHED();
  }

  controller_->RemoveObserver(this);
  controller_->ResetIdleState();

  FireWebUIListener("chrome-cleanup-on-dismiss");
}

void ChromeCleanupHandler::HandleRegisterChromeCleanerObserver(
    const base::ListValue* args) {
  DCHECK_EQ(0U, args->GetSize());

  UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.CleanupCard", true);
  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.CleanupWebui_Shown"));
  AllowJavascript();

  // Send the current logs upload state.
  OnLogsEnabledChanged(controller_->logs_enabled());
}

void ChromeCleanupHandler::HandleStartScanning(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  bool allow_logs_upload = false;
  args->GetBoolean(0, &allow_logs_upload);

  // The state is propagated to all open tabs and should be consistent.
  DCHECK_EQ(controller_->logs_enabled(), allow_logs_upload);

  // TODO(crbug.com/776538): Force a proper on-demand update of the component.
  component_updater::RegisterSwReporterComponent(
      g_browser_process->component_updater());

  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.CleanupWebui_StartScanning"));
}

void ChromeCleanupHandler::HandleRestartComputer(const base::ListValue* args) {
  DCHECK_EQ(0U, args->GetSize());

  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.CleanupWebui_RestartComputer"));

  FireWebUIListener("chrome-cleanup-on-dismiss");

  controller_->Reboot();
}

void ChromeCleanupHandler::HandleSetLogsUploadPermission(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  bool allow_logs_upload = false;
  args->GetBoolean(0, &allow_logs_upload);

  if (allow_logs_upload) {
    base::RecordAction(base::UserMetricsAction(
        "SoftwareReporter.CleanupWebui_LogsUploadEnabled"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "SoftwareReporter.CleanupWebui_LogsUploadDisabled"));
  }

  controller_->SetLogsEnabled(allow_logs_upload);
}

void ChromeCleanupHandler::HandleStartCleanup(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  bool allow_logs_upload = false;
  args->GetBoolean(0, &allow_logs_upload);

  // The state is propagated to all open tabs and should be consistent.
  DCHECK_EQ(controller_->logs_enabled(), allow_logs_upload);

  safe_browsing::RecordCleanupStartedHistogram(
      safe_browsing::CLEANUP_STARTED_FROM_PROMPT_IN_SETTINGS);
  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.CleanupWebui_StartCleanup"));

  controller_->ReplyWithUserResponse(
      profile_,
      allow_logs_upload
          ? ChromeCleanerController::UserResponse::kAcceptedWithLogs
          : ChromeCleanerController::UserResponse::kAcceptedWithoutLogs);
}

void ChromeCleanupHandler::HandleNotifyShowDetails(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  bool details_section_visible = false;
  args->GetBoolean(0, &details_section_visible);

  if (details_section_visible) {
    base::RecordAction(
        base::UserMetricsAction("SoftwareReporter.CleanupWebui_ShowDetails"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("SoftwareReporter.CleanupWebui_HideDetails"));
  }
}

void ChromeCleanupHandler::HandleNotifyChromeCleanupLearnMoreClicked(
    const base::ListValue* args) {
  CHECK_EQ(0U, args->GetSize());

  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.CleanupWebui_LearnMore"));
}

}  // namespace settings
