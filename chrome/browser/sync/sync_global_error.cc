// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_global_error.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

SyncGlobalError::SyncGlobalError(ProfileSyncService* service,
                                 SigninManager* signin)
    : service_(service),
      signin_(signin) {
  DCHECK(service_);
  DCHECK(signin_);
  OnStateChanged();
}

SyncGlobalError::~SyncGlobalError() {
}

bool SyncGlobalError::HasBadge() {
  return !menu_label_.empty();
}

bool SyncGlobalError::HasMenuItem() {
  // When we're on Chrome OS we need to add a separate menu item to the wrench
  // menu to the show the error. On other platforms we can just reuse the
  // "Sign in to Chrome..." menu item to show the error.
#if defined(OS_CHROMEOS)
  return !menu_label_.empty();
#else
  return false;
#endif
}

int SyncGlobalError::MenuItemCommandID() {
  return IDC_SHOW_SIGNIN_ERROR;
}

string16 SyncGlobalError::MenuItemLabel() {
  return menu_label_;
}

void SyncGlobalError::ExecuteMenuItem(Browser* browser) {
  LoginUIService* login_ui = LoginUIServiceFactory::GetForProfile(
      service_->profile());
  if (login_ui->current_login_ui()) {
    login_ui->current_login_ui()->FocusUI();
    return;
  }
  // Need to navigate to the settings page and display the UI.
  chrome::ShowSettingsSubPage(browser, chrome::kSyncSetupSubPage);
}

bool SyncGlobalError::HasBubbleView() {
  return !bubble_message_.empty() && !bubble_accept_label_.empty();
}

string16 SyncGlobalError::GetBubbleViewTitle() {
  return l10n_util::GetStringUTF16(IDS_SYNC_ERROR_BUBBLE_VIEW_TITLE);
}

string16 SyncGlobalError::GetBubbleViewMessage() {
  return bubble_message_;
}

string16 SyncGlobalError::GetBubbleViewAcceptButtonLabel() {
  return bubble_accept_label_;
}

string16 SyncGlobalError::GetBubbleViewCancelButtonLabel() {
  return string16();
}

void SyncGlobalError::OnBubbleViewDidClose(Browser* browser) {
}

void SyncGlobalError::BubbleViewAcceptButtonPressed(Browser* browser) {
  ExecuteMenuItem(browser);
}

void SyncGlobalError::BubbleViewCancelButtonPressed(Browser* browser) {
  NOTREACHED();
}

void SyncGlobalError::OnStateChanged() {
  string16 menu_label;
  string16 bubble_message;
  string16 bubble_accept_label;
  sync_ui_util::GetStatusLabelsForSyncGlobalError(
      service_, *signin_, &menu_label, &bubble_message, &bubble_accept_label);

  // All the labels should be empty or all of them non-empty.
  DCHECK((menu_label.empty() && bubble_message.empty() &&
          bubble_accept_label.empty()) ||
         (!menu_label.empty() && !bubble_message.empty() &&
          !bubble_accept_label.empty()));

  if (menu_label != menu_label_ || bubble_message != bubble_message_ ||
      bubble_accept_label != bubble_accept_label_) {
    menu_label_ = menu_label;
    bubble_message_ = bubble_message;
    bubble_accept_label_ = bubble_accept_label;

    // Profile can be NULL during tests.
    Profile* profile = service_->profile();
    if (profile) {
      GlobalErrorServiceFactory::GetForProfile(
          profile)->NotifyErrorsChanged(this);
    }
  }
}
