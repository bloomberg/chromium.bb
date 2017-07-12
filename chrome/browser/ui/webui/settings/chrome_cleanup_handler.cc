// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chrome_cleanup_handler.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

using safe_browsing::ChromeCleanerController;

namespace settings {

namespace {

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
      base::Bind(&ChromeCleanupHandler::HandleDismiss, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "registerChromeCleanerObserver",
      base::Bind(&ChromeCleanupHandler::HandleRegisterChromeCleanerObserver,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "restartComputer",
      base::Bind(&ChromeCleanupHandler::HandleRestartComputer,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setLogsUploadPermission",
      base::Bind(&ChromeCleanupHandler::HandleSetLogsUploadPermission,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startCleanup", base::Bind(&ChromeCleanupHandler::HandleStartCleanup,
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
  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::Value("chrome-cleanup-on-idle"),
                         base::Value(IdleReasonToString(idle_reason)));
}

void ChromeCleanupHandler::OnScanning() {
  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::Value("chrome-cleanup-on-scanning"));
}

void ChromeCleanupHandler::OnInfected(const std::set<base::FilePath>& files) {
  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::Value("chrome-cleanup-on-infected"),
                         GetFilesAsListStorage(files));
}

void ChromeCleanupHandler::OnCleaning(const std::set<base::FilePath>& files) {
  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::Value("chrome-cleanup-on-cleaning"),
                         GetFilesAsListStorage(files));
}

void ChromeCleanupHandler::OnRebootRequired() {
  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::Value("chrome-cleanup-on-reboot-required"));
}

void ChromeCleanupHandler::OnLogsEnabledChanged(bool logs_enabled) {
  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::Value("chrome-cleanup-upload-permission-change"),
                         base::Value(logs_enabled));
}

void ChromeCleanupHandler::HandleDismiss(const base::ListValue* args) {
  DCHECK_EQ(0U, args->GetSize());

  controller_->RemoveObserver(this);
  controller_->ResetIdleState();

  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::Value("chrome-cleanup-on-dismiss"));
}

void ChromeCleanupHandler::HandleRegisterChromeCleanerObserver(
    const base::ListValue* args) {
  DCHECK_EQ(0U, args->GetSize());
  // The Polymer element should never be attached if the feature is
  // disabled.
  DCHECK(
      base::FeatureList::IsEnabled(safe_browsing::kInBrowserCleanerUIFeature));

  UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.CleanupCard", true);
  AllowJavascript();

  // Send the current logs upload state.
  OnLogsEnabledChanged(controller_->logs_enabled());
}

void ChromeCleanupHandler::HandleRestartComputer(const base::ListValue* args) {
  DCHECK_EQ(0U, args->GetSize());

  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::Value("chrome-cleanup-on-dismiss"));

  controller_->Reboot();
}

void ChromeCleanupHandler::HandleSetLogsUploadPermission(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  bool allow_logs_upload = false;
  args->GetBoolean(0, &allow_logs_upload);

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
  controller_->ReplyWithUserResponse(
      profile_,
      allow_logs_upload
          ? ChromeCleanerController::UserResponse::kAcceptedWithLogs
          : ChromeCleanerController::UserResponse::kAcceptedWithoutLogs);
}

}  // namespace settings
