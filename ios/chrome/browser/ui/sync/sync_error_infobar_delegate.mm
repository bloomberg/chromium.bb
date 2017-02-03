// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/sync/sync_error_infobar_delegate.h"

#import <UIKit/UIKit.h>

#include <utility>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#import "ios/chrome/browser/ui/sync/sync_util.h"
#include "ui/base/l10n/l10n_util.h"

// static
bool SyncErrorInfoBarDelegate::Create(infobars::InfoBarManager* infobar_manager,
                                      ios::ChromeBrowserState* browser_state) {
  DCHECK(infobar_manager);
  std::unique_ptr<ConfirmInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(browser_state));
  return !!infobar_manager->AddInfoBar(
      infobar_manager->CreateConfirmInfoBar(std::move(delegate)));
}

SyncErrorInfoBarDelegate::SyncErrorInfoBarDelegate(
    ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {
  DCHECK(!browser_state->IsOffTheRecord());
  icon_ = gfx::Image([UIImage imageNamed:@"infobar_warning"]);
  SyncSetupService* sync_setup_service =
      SyncSetupServiceFactory::GetForBrowserState(browser_state);
  DCHECK(sync_setup_service);
  // Set all of the UI based on the sync state at the same time to ensure
  // they all correspond to the same sync error.
  error_state_ = sync_setup_service->GetSyncServiceState();
  message_ = base::SysNSStringToUTF16(
      ios_internal::sync::GetSyncErrorMessageForBrowserState(browser_state_));
  button_text_ = base::SysNSStringToUTF16(
      ios_internal::sync::GetSyncErrorButtonTitleForBrowserState(
          browser_state_));
  command_.reset([ios_internal::sync::GetSyncCommandForBrowserState(
      browser_state_) retain]);

  // Register for sync status changes.
  syncer::SyncService* sync_service =
      IOSChromeProfileSyncServiceFactory::GetForBrowserState(browser_state_);
  sync_service->AddObserver(this);
}

SyncErrorInfoBarDelegate::~SyncErrorInfoBarDelegate() {
  syncer::SyncService* sync_service =
      IOSChromeProfileSyncServiceFactory::GetForBrowserState(browser_state_);
  sync_service->RemoveObserver(this);
}

infobars::InfoBarDelegate::InfoBarIdentifier
SyncErrorInfoBarDelegate::GetIdentifier() const {
  return SYNC_ERROR_INFOBAR_DELEGATE;
}

base::string16 SyncErrorInfoBarDelegate::GetMessageText() const {
  return message_;
}

int SyncErrorInfoBarDelegate::GetButtons() const {
  return button_text_.empty() ? BUTTON_NONE : BUTTON_OK;
}

base::string16 SyncErrorInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK(button == BUTTON_OK);
  return button_text_;
}

gfx::Image SyncErrorInfoBarDelegate::GetIcon() const {
  return icon_;
}

bool SyncErrorInfoBarDelegate::Accept() {
  DCHECK(command_);
  UIWindow* main_window = [[UIApplication sharedApplication] keyWindow];
  DCHECK(main_window);
  [main_window chromeExecuteCommand:command_];
  return false;
}

void SyncErrorInfoBarDelegate::OnStateChanged(syncer::SyncService* sync) {
  // If the inforbar is in the process of being removed, nothing must be done.
  infobars::InfoBar* infobar = this->infobar();
  if (!infobar)
    return;
  SyncSetupService* sync_setup_service =
      SyncSetupServiceFactory::GetForBrowserState(browser_state_);
  SyncSetupService::SyncServiceState new_error_state =
      sync_setup_service->GetSyncServiceState();
  if (error_state_ == new_error_state)
    return;
  error_state_ = new_error_state;
  if (ios_internal::sync::IsTransientSyncError(new_error_state)) {
    infobar->RemoveSelf();
  } else {
    infobars::InfoBarManager* infobar_manager = infobar->owner();
    if (infobar_manager) {
      std::unique_ptr<ConfirmInfoBarDelegate> new_infobar_delegate(
          new SyncErrorInfoBarDelegate(browser_state_));
      infobar_manager->ReplaceInfoBar(infobar,
                                      infobar_manager->CreateConfirmInfoBar(
                                          std::move(new_infobar_delegate)));
    }
  }
}
