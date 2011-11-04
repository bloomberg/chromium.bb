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
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace protector {

namespace {

// Timeout before the global error is removed (wrench menu item disappears).
const int kMenuItemDisplayPeriodMs = 10*60*1000;  // 10 min

// IDs of menu item labels.
const int kMenuItemLabelIDs[] = {
  IDS_SEARCH_ENGINE_CHANGE_WRENCH_MENU_ITEM,
  IDS_HOMEPAGE_CHANGE_WRENCH_MENU_ITEM
};

// IDs of bubble title messages.
const int kBubbleTitleIDs[] = {
  IDS_SEARCH_ENGINE_CHANGE_BUBBLE_TITLE,
  IDS_HOMEPAGE_CHANGE_BUBBLE_TITLE
};

// IDs of bubble text messages.
const int kBubbleMessageIDs[] = {
  IDS_SEARCH_ENGINE_CHANGE_BUBBLE_TEXT,
  IDS_HOMEPAGE_CHANGE_BUBBLE_TEXT
};

// IDs of bubble text messages when the old setting is unknown.
const int kBubbleMessageOldUnknownIDs[] = {
  IDS_SEARCH_ENGINE_CHANGE_UNKNOWN_BUBBLE_TEXT,
  IDS_HOMEPAGE_CHANGE_UNKNOWN_BUBBLE_TEXT
};

// IDs of "Keep Setting" button titles.
const int kBubbleKeepSettingIDs[] = {
  IDS_SEARCH_ENGINE_CHANGE_RESTORE,
  IDS_HOMEPAGE_CHANGE_RESTORE
};

// IDs of "Change Setting" button titles.
const int kBubbleChangeSettingIDs[] = {
  IDS_SEARCH_ENGINE_CHANGE_APPLY,
  IDS_HOMEPAGE_CHANGE_APPLY
};

}  // namespace

SettingsChangeGlobalError::SettingsChangeGlobalError(
    const SettingChangeVector& changes,
    SettingsChangeGlobalErrorDelegate* delegate)
    : changes_(changes),
      delegate_(delegate),
      profile_(NULL),
      browser_(NULL),
      closed_by_button_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK(changes.size() > 0);
}

SettingsChangeGlobalError::~SettingsChangeGlobalError() {
  STLDeleteElements(&changes_);
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

// TODO(ivankr): Currently the menu item/bubble only displays a warning about
// the first change. We want to fix this so that a single menu item/bubble
// can display warning about multiple changes.

string16 SettingsChangeGlobalError::MenuItemLabel() {
  return l10n_util::GetStringUTF16(kMenuItemLabelIDs[changes_.front()->type()]);
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
  return l10n_util::GetStringUTF16(kBubbleTitleIDs[changes_.front()->type()]);
}

string16 SettingsChangeGlobalError::GetBubbleViewMessage() {
  SettingChange* change = changes_.front();
  const string16& old_setting = change->GetOldSetting();
  if (old_setting.empty()) {
    return l10n_util::GetStringFUTF16(
        kBubbleMessageOldUnknownIDs[change->type()],
        change->GetNewSetting());
  } else {
    return l10n_util::GetStringFUTF16(
        kBubbleMessageIDs[change->type()],
        old_setting,
        change->GetNewSetting());
  }
}

string16 SettingsChangeGlobalError::GetBubbleViewAcceptButtonLabel() {
  SettingChange* change = changes_.front();
  string16 old_setting = change->GetOldSetting();
  if (old_setting.empty()) {
    return l10n_util::GetStringUTF16(IDS_SETTINGS_CHANGE_OPEN_SETTINGS);
  } else {
    return l10n_util::GetStringFUTF16(
        kBubbleKeepSettingIDs[change->type()],
        old_setting);
  }
}

string16 SettingsChangeGlobalError::GetBubbleViewCancelButtonLabel() {
  SettingChange* change = changes_.front();
  return l10n_util::GetStringFUTF16(kBubbleChangeSettingIDs[change->type()],
                                    change->GetNewSetting());
}

void SettingsChangeGlobalError::BubbleViewAcceptButtonPressed() {
  closed_by_button_ = true;
  DCHECK(delegate_);
  VLOG(1) << "Discard changes";
  delegate_->OnDiscardChanges();
}

void SettingsChangeGlobalError::BubbleViewCancelButtonPressed() {
  closed_by_button_ = true;
  DCHECK(delegate_);
  VLOG(1) << "Apply changes";
  delegate_->OnApplyChanges();
}

void SettingsChangeGlobalError::RemoveFromProfile() {
  DCHECK(delegate_);
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
