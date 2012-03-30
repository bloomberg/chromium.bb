// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/protector_service.h"

#include "base/logging.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/protector/settings_change_global_error.h"
#include "chrome/browser/protector/protected_prefs_watcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_source.h"

namespace protector {

ProtectorService::ProtectorService(Profile* profile)
    : profile_(profile),
      has_active_change_(false) {
  // Start observing pref changes.
  prefs_watcher_.reset(new ProtectedPrefsWatcher(profile));
}

ProtectorService::~ProtectorService() {
  DCHECK(!IsShowingChange());  // Should have been dismissed by Shutdown.
}

void ProtectorService::ShowChange(BaseSettingChange* change) {
  DCHECK(change);
  Item new_item;
  new_item.change.reset(change);
  DVLOG(1) << "Init change";
  if (!change->Init(profile_)) {
    LOG(WARNING) << "Error while initializing, dismissing change";
    return;
  }
  SettingsChangeGlobalError* error =
      new SettingsChangeGlobalError(change, this);
  new_item.error.reset(error);
  items_.push_back(new_item);
  // Do not show the bubble immediately if another one is active.
  error->AddToProfile(profile_, !has_active_change_);
  has_active_change_ = true;
}

bool ProtectorService::IsShowingChange() const {
  return !items_.empty();
}

void ProtectorService::ApplyChange(BaseSettingChange* change,
                                   Browser* browser) {
  change->Apply(browser);
  DismissChange(change);
}

void ProtectorService::DiscardChange(BaseSettingChange* change,
                                     Browser* browser) {
  change->Discard(browser);
  DismissChange(change);
}

void ProtectorService::DismissChange(BaseSettingChange* change) {
  std::vector<Item>::iterator item =
      std::find_if(items_.begin(), items_.end(), MatchItemByChange(change));
  DCHECK(item != items_.end());
  item->error->RemoveFromProfile();
}

void ProtectorService::OpenTab(const GURL& url, Browser* browser) {
  DCHECK(browser);
  browser->ShowSingletonTab(url);
}

ProtectedPrefsWatcher* ProtectorService::GetPrefsWatcher() {
  return prefs_watcher_.get();
}

void ProtectorService::Shutdown() {
  while (IsShowingChange())
    items_[0].error->RemoveFromProfile();
}

void ProtectorService::OnApplyChange(SettingsChangeGlobalError* error,
                                     Browser* browser) {
  DVLOG(1) << "Apply change";
  error->change()->Apply(browser);
  has_active_change_ = false;
}

void ProtectorService::OnDiscardChange(SettingsChangeGlobalError* error,
                                       Browser* browser) {
  DVLOG(1) << "Discard change";
  error->change()->Discard(browser);
  has_active_change_ = false;
}

void ProtectorService::OnDecisionTimeout(SettingsChangeGlobalError* error) {
  DVLOG(1) << "Timeout";
  error->change()->Timeout();
}

void ProtectorService::OnRemovedFromProfile(SettingsChangeGlobalError* error) {
  std::vector<Item>::iterator item =
      std::find_if(items_.begin(), items_.end(), MatchItemByError(error));
  DCHECK(item != items_.end());
  items_.erase(item);

  if (!has_active_change_) {
    // If there are changes that haven't been shown yet, show the first one.
    for (item = items_.begin(); item != items_.end(); ++item) {
      if (!item->error->HasShownBubbleView()) {
        item->error->ShowBubble();
        has_active_change_ = true;
        return;
      }
    }
  }
}

BaseSettingChange* ProtectorService::GetLastChange() {
  return items_.empty() ? NULL : items_.back().change.get();
}

ProtectorService::Item::Item() {
}

ProtectorService::Item::~Item() {
}

ProtectorService::MatchItemByChange::MatchItemByChange(
    const BaseSettingChange* other) : other_(other) {
}

bool ProtectorService::MatchItemByChange::operator()(
    const ProtectorService::Item& item) {
  return other_ == item.change.get();
}

ProtectorService::MatchItemByError::MatchItemByError(
    const SettingsChangeGlobalError* other) : other_(other) {
}

bool ProtectorService::MatchItemByError::operator()(
    const ProtectorService::Item& item) {
  return other_ == item.error.get();
}

}  // namespace protector
