// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/settings_change_global_error.h"

#include "base/bind.h"
#include "base/task.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/global_error_service.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

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
    const ChangesVector& changes,
    const base::Closure& apply_changes_cb,
    const base::Closure& revert_changes_cb)
    : changes_(changes),
      apply_changes_cb_(apply_changes_cb),
      revert_changes_cb_(revert_changes_cb),
      profile_(NULL),
      closed_by_button_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK(changes.size() > 0);
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

// TODO(ivankr): Currently the menu item/bubble only displays a warning about
// the first change. We want to fix this so that a single menu item/bubble
// can display warning about multiple changes.

string16 SettingsChangeGlobalError::MenuItemLabel() {
  return l10n_util::GetStringUTF16(kMenuItemLabelIDs[changes_.front().type]);
}

void SettingsChangeGlobalError::ExecuteMenuItem(Browser* browser) {
  weak_factory_.InvalidateWeakPtrs();  // Cancel previously posted tasks.
  ShowBubbleView(browser);
}

bool SettingsChangeGlobalError::HasBubbleView() {
  return true;
}

string16 SettingsChangeGlobalError::GetBubbleViewTitle() {
  return l10n_util::GetStringUTF16(kBubbleTitleIDs[changes_.front().type]);
}

string16 SettingsChangeGlobalError::GetBubbleViewMessage() {
  const Change& change = changes_.front();
  return change.old_setting.empty() ?
      l10n_util::GetStringFUTF16(kBubbleMessageOldUnknownIDs[change.type],
                                 change.new_setting) :
      l10n_util::GetStringFUTF16(kBubbleMessageIDs[change.type],
                                 change.old_setting, change.new_setting);
}

string16 SettingsChangeGlobalError::GetBubbleViewAcceptButtonLabel() {
  const Change& change = changes_.front();
  return l10n_util::GetStringFUTF16(kBubbleChangeSettingIDs[change.type],
                                    change.new_setting);
}

string16 SettingsChangeGlobalError::GetBubbleViewCancelButtonLabel() {
  const Change& change = changes_.front();
  return change.old_setting.empty() ?
      l10n_util::GetStringUTF16(IDS_SETTINGS_CHANGE_OPEN_SETTINGS) :
      l10n_util::GetStringFUTF16(kBubbleKeepSettingIDs[change.type],
                                 change.old_setting);
}

bool SettingsChangeGlobalError::IsAcceptButtonDefault() {
  return false;
}

void SettingsChangeGlobalError::BubbleViewAcceptButtonPressed() {
  VLOG(1) << "Apply changes";
  apply_changes_cb_.Run();
  closed_by_button_ = true;
}

void SettingsChangeGlobalError::BubbleViewCancelButtonPressed() {
  VLOG(1) << "Revert changes";
  revert_changes_cb_.Run();
  closed_by_button_ = true;
}

void SettingsChangeGlobalError::RemoveFromProfile() {
  if (profile_)
    GlobalErrorServiceFactory::GetForProfile(profile_)->RemoveGlobalError(this);
  BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
}

void SettingsChangeGlobalError::BubbleViewDidClose() {
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

void SettingsChangeGlobalError::ShowForDefaultProfile() {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    AddToDefaultProfile();
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SettingsChangeGlobalError::AddToDefaultProfile,
                   base::Unretained(this)));
  }
}

void SettingsChangeGlobalError::AddToDefaultProfile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_ = ProfileManager::GetDefaultProfile();
  GlobalErrorServiceFactory::GetForProfile(profile_)->AddGlobalError(this);
  Show();
}

void SettingsChangeGlobalError::Show() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (browser)
    ShowBubbleView(browser);
}
