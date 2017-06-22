// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_dialog_controller_impl_win.h"

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

void ChromeCleanerDialogControllerImpl::Accept() {
  DCHECK(browser_);

  cleaner_controller_->ReplyWithUserResponse(
      browser_->profile(), ChromeCleanerController::UserResponse::kAccepted);
  OpenSettingsPage(browser_);
  OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::Cancel() {
  DCHECK(browser_);

  cleaner_controller_->ReplyWithUserResponse(
      browser_->profile(), ChromeCleanerController::UserResponse::kDenied);
  OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::Close() {
  DCHECK(browser_);

  cleaner_controller_->ReplyWithUserResponse(
      browser_->profile(), ChromeCleanerController::UserResponse::kDismissed);
  OnInteractionDone();
}

void ChromeCleanerDialogControllerImpl::DetailsButtonClicked() {
  OpenSettingsPage(browser_);
  OnInteractionDone();
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
    OnInteractionDone();
    return;
  }

  chrome::ShowChromeCleanerPrompt(browser_, this);
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
