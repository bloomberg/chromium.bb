// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_global_error.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"

SyncGlobalError::SyncGlobalError(SyncErrorController* error_controller,
                                 ProfileSyncService* profile_sync_service)
    : error_controller_(error_controller),
      service_(profile_sync_service) {
  DCHECK(service_);
  error_controller_->AddObserver(this);
  GlobalErrorServiceFactory::GetForProfile(service_->profile())->
      AddGlobalError(this);
}

SyncGlobalError::~SyncGlobalError() {
  DCHECK(!error_controller_)
      << "SyncGlobalError::Shutdown() was not called";
}

void SyncGlobalError::Shutdown() {
  GlobalErrorServiceFactory::GetForProfile(service_->profile())->
      RemoveGlobalError(this);
  error_controller_->RemoveObserver(this);
  error_controller_ = NULL;
}

bool SyncGlobalError::HasMenuItem() {
  return !menu_label_.empty();
}

int SyncGlobalError::MenuItemCommandID() {
  return IDC_SHOW_SYNC_ERROR;
}

base::string16 SyncGlobalError::MenuItemLabel() {
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

base::string16 SyncGlobalError::GetBubbleViewTitle() {
  return l10n_util::GetStringUTF16(IDS_SYNC_ERROR_BUBBLE_VIEW_TITLE);
}

std::vector<base::string16> SyncGlobalError::GetBubbleViewMessages() {
  return std::vector<base::string16>(1, bubble_message_);
}

base::string16 SyncGlobalError::GetBubbleViewAcceptButtonLabel() {
  return bubble_accept_label_;
}

base::string16 SyncGlobalError::GetBubbleViewCancelButtonLabel() {
  return base::string16();
}

void SyncGlobalError::OnBubbleViewDidClose(Browser* browser) {
}

void SyncGlobalError::BubbleViewAcceptButtonPressed(Browser* browser) {
  ExecuteMenuItem(browser);
}

void SyncGlobalError::BubbleViewCancelButtonPressed(Browser* browser) {
  NOTREACHED();
}

void SyncGlobalError::OnErrorChanged() {
  base::string16 menu_label;
  base::string16 bubble_message;
  base::string16 bubble_accept_label;
  sync_ui_util::GetStatusLabelsForSyncGlobalError(
      service_, &menu_label, &bubble_message, &bubble_accept_label);

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
