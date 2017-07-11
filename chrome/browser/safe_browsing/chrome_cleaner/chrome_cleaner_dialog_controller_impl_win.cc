// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_dialog_controller_impl_win.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

void OpenSettingsPage(Browser* browser) {
  browser->OpenURL(content::OpenURLParams(
      GURL(chrome::kChromeUISettingsURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, /*is_renderer_initiated=*/false));
}

Browser* FindBrowserForDialog() {
  BrowserList* browser_list = BrowserList::GetInstance();
  for (BrowserList::const_reverse_iterator browser_iterator =
           browser_list->begin_last_active();
       browser_iterator != browser_list->end_last_active();
       ++browser_iterator) {
    Browser* browser = *browser_iterator;
    if (browser->window()->IsActive() || !browser->window()->IsMinimized())
      return browser;
  }

  return nullptr;
}

// These values are used to send UMA information and are replicated in the
// histograms.xml file, so the order MUST NOT CHANGE.
enum PromptDialogResponseHistogramValue {
  PROMPT_DIALOG_RESPONSE_ACCEPTED = 0,
  PROMPT_DIALOG_RESPONSE_DETAILS = 1,
  PROMPT_DIALOG_RESPONSE_CANCELLED = 2,
  PROMPT_DIALOG_RESPONSE_DISMISSED = 3,

  PROMPT_DIALOG_RESPONSE_MAX,
};

void RecordPromptDialogResponseHistogram(
    PromptDialogResponseHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.PromptDialogResponse", value,
                            PROMPT_DIALOG_RESPONSE_MAX);
}

}  // namespace

ChromeCleanerDialogControllerImpl::ChromeCleanerDialogControllerImpl(
    ChromeCleanerController* cleaner_controller)
    : cleaner_controller_(cleaner_controller) {
  DCHECK(cleaner_controller_);
  DCHECK_EQ(ChromeCleanerController::State::kScanning,
            cleaner_controller_->state());

  cleaner_controller_->AddObserver(this);
}

ChromeCleanerDialogControllerImpl::~ChromeCleanerDialogControllerImpl() =
    default;

void ChromeCleanerDialogControllerImpl::DialogShown() {}

void ChromeCleanerDialogControllerImpl::Accept(bool logs_enabled) {
  DCHECK(browser_);

  RecordPromptDialogResponseHistogram(PROMPT_DIALOG_RESPONSE_ACCEPTED);
  RecordCleanupStartedHistogram(CLEANUP_STARTED_FROM_PROMPT_DIALOG);

  cleaner_controller_->ReplyWithUserResponse(
      browser_->profile(),
      logs_enabled
          ? ChromeCleanerController::UserResponse::kAcceptedWithLogs
          : ChromeCleanerController::UserResponse::kAcceptedWithoutLogs);
  OpenSettingsPage(browser_);
  OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::Cancel() {
  DCHECK(browser_);

  RecordPromptDialogResponseHistogram(PROMPT_DIALOG_RESPONSE_CANCELLED);

  cleaner_controller_->ReplyWithUserResponse(
      browser_->profile(), ChromeCleanerController::UserResponse::kDenied);
  OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::Close() {
  DCHECK(browser_);

  RecordPromptDialogResponseHistogram(PROMPT_DIALOG_RESPONSE_DISMISSED);

  cleaner_controller_->ReplyWithUserResponse(
      browser_->profile(), ChromeCleanerController::UserResponse::kDismissed);
  OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::DetailsButtonClicked(
    bool logs_enabled) {
  RecordPromptDialogResponseHistogram(PROMPT_DIALOG_RESPONSE_DETAILS);

  cleaner_controller_->SetLogsEnabled(logs_enabled);
  OpenSettingsPage(browser_);
  OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::SetLogsEnabled(bool logs_enabled) {
  cleaner_controller_->SetLogsEnabled(logs_enabled);
}

bool ChromeCleanerDialogControllerImpl::LogsEnabled() {
  return cleaner_controller_->logs_enabled();
}

void ChromeCleanerDialogControllerImpl::OnIdle(
    ChromeCleanerController::IdleReason idle_reason) {
  if (!dialog_shown_)
    OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::OnScanning() {
  // This notification is received when the object is first added as an observer
  // of cleaner_controller_.
  //
  // TODO(alito): Close the dialog in case it has been kept open until the next
  // time the prompt is going to be shown. http://crbug.com/734689
  DCHECK(!dialog_shown_);
}

void ChromeCleanerDialogControllerImpl::OnInfected(
    const std::set<base::FilePath>& files_to_delete) {
  DCHECK(!dialog_shown_);

  browser_ = FindBrowserForDialog();
  if (!browser_) {
    // TODO(alito): Register with chrome::BrowserListObserver to get notified
    // later if a suitable browser window becomes available to show the
    // prompt. http://crbug.com/734677
    RecordPromptNotShownWithReasonHistogram(
        NO_PROMPT_REASON_BROWSER_NOT_AVAILABLE);
    OnInteractionDone();
    return;
  }

  chrome::ShowChromeCleanerPrompt(browser_, this);
  RecordPromptShownHistogram();
  dialog_shown_ = true;
}

void ChromeCleanerDialogControllerImpl::OnCleaning(
    const std::set<base::FilePath>& files_to_delete) {
  if (!dialog_shown_)
    OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::OnRebootRequired() {
  if (!dialog_shown_)
    OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::OnInteractionDone() {
  cleaner_controller_->RemoveObserver(this);
  delete this;
}

}  // namespace safe_browsing
