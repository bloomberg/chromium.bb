// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/settings_change_global_error.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/protector/settings_change_global_error_delegate.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/global_error_service.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace protector {

namespace {

// Timeout before the global error is removed (wrench menu item disappears).
const int kMenuItemDisplayPeriodMs = 10*60*1000;  // 10 min

}  // namespace

SettingsChangeGlobalError::SettingsChangeGlobalError(
    BaseSettingChange* change,
    SettingsChangeGlobalErrorDelegate* delegate)
    : change_(change),
      delegate_(delegate),
      profile_(NULL),
      browser_(NULL),
      closed_by_button_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK(delegate_);
}

SettingsChangeGlobalError::~SettingsChangeGlobalError() {
}

bool SettingsChangeGlobalError::HasBadge() {
  return true;
}

bool SettingsChangeGlobalError::HasMenuItem() {
  return true;
}

int SettingsChangeGlobalError::MenuItemCommandID() {
  return IDC_SHOW_SETTINGS_CHANGES;
}

string16 SettingsChangeGlobalError::MenuItemLabel() {
  return change_->GetBubbleTitle();
}

void SettingsChangeGlobalError::ExecuteMenuItem(Browser* browser) {
  // Cancel previously posted tasks.
  weak_factory_.InvalidateWeakPtrs();
  browser_ = browser;
  ShowBubbleView(browser_);
}

bool SettingsChangeGlobalError::HasBubbleView() {
  return true;
}

string16 SettingsChangeGlobalError::GetBubbleViewTitle() {
  return change_->GetBubbleTitle();
}

string16 SettingsChangeGlobalError::GetBubbleViewMessage() {
  return change_->GetBubbleMessage();
}

// The Accept and Revert buttons are swapped like the 'server' and 'client'
// concepts in X11. Accept button (the default one) discards changes
// (keeps using previous setting) while cancel button applies changes
// (switches to the new setting). This is sick and blows my mind. - ivankr

string16 SettingsChangeGlobalError::GetBubbleViewAcceptButtonLabel() {
  return change_->GetDiscardButtonText();
}

string16 SettingsChangeGlobalError::GetBubbleViewCancelButtonLabel() {
  return change_->GetApplyButtonText();
}

void SettingsChangeGlobalError::BubbleViewAcceptButtonPressed() {
  closed_by_button_ = true;
  delegate_->OnDiscardChange();
}

void SettingsChangeGlobalError::BubbleViewCancelButtonPressed() {
  closed_by_button_ = true;
  delegate_->OnApplyChange();
}

void SettingsChangeGlobalError::RemoveFromProfile() {
  if (profile_)
    GlobalErrorServiceFactory::GetForProfile(profile_)->RemoveGlobalError(this);
  if (!closed_by_button_)
    delegate_->OnDecisionTimeout();
  delegate_->OnRemovedFromProfile();
}

void SettingsChangeGlobalError::BubbleViewDidClose() {
  browser_ = NULL;
  if (!closed_by_button_) {
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SettingsChangeGlobalError::RemoveFromProfile,
                   weak_factory_.GetWeakPtr()),
        kMenuItemDisplayPeriodMs);
  } else {
    RemoveFromProfile();
  }
}

void SettingsChangeGlobalError::ShowForProfile(Profile* profile) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    AddToProfile(profile);
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SettingsChangeGlobalError::AddToProfile,
                   base::Unretained(this),
                   profile));
  }
}

void SettingsChangeGlobalError::AddToProfile(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_ = profile;
  GlobalErrorServiceFactory::GetForProfile(profile_)->AddGlobalError(this);
  Show();
}

void SettingsChangeGlobalError::Show() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  browser_ = BrowserList::GetLastActiveWithProfile(profile_);
  if (browser_)
    ShowBubbleView(browser_);
}

}  // namespace protector
