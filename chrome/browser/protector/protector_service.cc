// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/protector_service.h"

#include "base/logging.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/protector/composite_settings_change.h"
#include "chrome/browser/protector/keys.h"
#include "chrome/browser/protector/protected_prefs_watcher.h"
#include "chrome/browser/protector/protector_utils.h"
#include "chrome/browser/protector/settings_change_global_error.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_source.h"
#include "net/base/registry_controlled_domain.h"

namespace protector {

namespace {

// Returns true if changes with URLs |url1| and |url2| can be merged.
bool CanMerge(const GURL& url1, const GURL& url2) {
  VLOG(1) << "Checking if can merge " << url1.spec() << " with " << url2.spec();
  // All Google URLs are considered the same one.
  if (google_util::IsGoogleHostname(url1.host(),
                                    google_util::DISALLOW_SUBDOMAIN)) {
    return google_util::IsGoogleHostname(url2.host(),
                                         google_util::DISALLOW_SUBDOMAIN);
  }
  // Otherwise URLs must have the same domain.
  return net::RegistryControlledDomainService::SameDomainOrHost(url1, url2);
}

}  // namespace

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
  // Change instance will either be owned by |this| or deleted after this call.
  scoped_ptr<BaseSettingChange> change_ptr(change);

  DVLOG(1) << "Init change";
  if (!protector::IsEnabled()) {
    change_ptr->InitWhenDisabled(profile_);
    return;
  } else if (!change_ptr->Init(profile_)) {
    LOG(WARNING) << "Error while initializing, dismissing change";
    return;
  }

  Item* item_to_merge_with = FindItemToMergeWith(change_ptr.get());
  if (item_to_merge_with) {
    // CompositeSettingsChange takes ownership of merged changes.
    BaseSettingChange* existing_change = item_to_merge_with->change.release();
    CompositeSettingsChange* merged_change =
        existing_change->MergeWith(change_ptr.release());
    item_to_merge_with->change.reset(merged_change);
    item_to_merge_with->was_merged = true;
    if (item_to_merge_with->error->GetBubbleView())
      item_to_merge_with->show_when_merged = true;
    // Remove old GlobalError instance. Later in OnRemovedFromProfile() a new
    // GlobalError instance will be created for the composite change.
    item_to_merge_with->error->RemoveFromProfile();
  } else if (change->IsUserVisible()) {
    Item new_item;
    SettingsChangeGlobalError* error =
        new SettingsChangeGlobalError(change_ptr.get(), this);
    new_item.error.reset(error);
    new_item.change.reset(change_ptr.release());
    items_.push_back(new_item);
    // Do not show the bubble immediately if another one is active.
    error->AddToProfile(profile_, !has_active_change_);
    has_active_change_ = true;
  } else {
    VLOG(1) << "Not showing a change because it's not user-visible.";
  }
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
  Items::iterator item = std::find_if(items_.begin(), items_.end(),
                                      MatchItemByChange(change));
  DCHECK(item != items_.end());
  item->error->RemoveFromProfile();
}

void ProtectorService::OpenTab(const GURL& url, Browser* browser) {
  DCHECK(browser);
  chrome::ShowSingletonTab(browser, url);
}

ProtectedPrefsWatcher* ProtectorService::GetPrefsWatcher() {
  return prefs_watcher_.get();
}

void ProtectorService::StopWatchingPrefsForTesting() {
  prefs_watcher_.reset();
}

ProtectorService::Item* ProtectorService::FindItemToMergeWith(
    const BaseSettingChange* change) {
  if (!change->CanBeMerged())
    return NULL;
  GURL url = change->GetNewSettingURL();
  for (Items::iterator item = items_.begin(); item != items_.end(); item++) {
    if (item->change->CanBeMerged() &&
        CanMerge(url, item->change->GetNewSettingURL()))
      return &*item;
  }
  return NULL;
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
  Items::iterator item = std::find_if(items_.begin(), items_.end(),
                                      MatchItemByError(error));
  DCHECK(item != items_.end());

  if (item->was_merged) {
    bool show_new_error = !has_active_change_ || item->show_when_merged;
    item->was_merged = false;
    item->show_when_merged = false;
    // Item was merged with another change instance and error has been removed,
    // create a new one for the composite change.
    item->error.reset(new SettingsChangeGlobalError(item->change.get(), this));
    item->error->AddToProfile(profile_, show_new_error);
    has_active_change_ = true;
    return;
  }

  items_.erase(item);

  // If no other change is shown and there are changes that haven't been shown
  // yet, show the first one.
  if (!has_active_change_) {
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

ProtectorService::Item::Item()
    : was_merged(false),
      show_when_merged(false) {
}

ProtectorService::Item::~Item() {
}

ProtectorService::MatchItemByChange::MatchItemByChange(
    const BaseSettingChange* other) : other_(other) {
}

bool ProtectorService::MatchItemByChange::operator()(
    const ProtectorService::Item& item) {
  return item.change->Contains(other_);
}

ProtectorService::MatchItemByError::MatchItemByError(
    const SettingsChangeGlobalError* other) : other_(other) {
}

bool ProtectorService::MatchItemByError::operator()(
    const ProtectorService::Item& item) {
  return other_ == item.error.get();
}

}  // namespace protector
