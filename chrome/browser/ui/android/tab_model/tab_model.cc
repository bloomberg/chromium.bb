// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/sync/glue/synced_window_delegate_android.h"
#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

using content::NotificationService;

TabModel::TabModel(Profile* profile)
  : profile_(profile),
    synced_window_delegate_(
        new browser_sync::SyncedWindowDelegateAndroid(this)),
    toolbar_model_(new ToolbarModelImpl(this)) {

  if (profile) {
    // A normal Profile creates an OTR profile if it does not exist when
    // GetOffTheRecordProfile() is called, so we guard it with
    // HasOffTheRecordProfile(). An OTR profile returns itself when you call
    // GetOffTheRecordProfile().
    is_off_the_record_ = (profile->HasOffTheRecordProfile() &&
        profile == profile->GetOffTheRecordProfile());

    // A profile can be destroyed, for example in the case of closing all
    // incognito tabs. We therefore must listen for when this happens, and
    // remove our pointer to the profile accordingly.
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                   content::Source<Profile>(profile_));
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                   content::NotificationService::AllSources());
  } else {
    is_off_the_record_ = false;
  }
}

TabModel::TabModel()
  : profile_(NULL),
    is_off_the_record_(false),
    synced_window_delegate_(
        new browser_sync::SyncedWindowDelegateAndroid(this)) {
}

TabModel::~TabModel() {
}

content::WebContents* TabModel::GetActiveWebContents() const {
  if (GetTabCount() == 0 || GetActiveIndex() < 0 ||
      GetActiveIndex() > GetTabCount())
    return NULL;
  return GetWebContentsAt(GetActiveIndex());
}

Profile* TabModel::GetProfile() const {
  return profile_;
}

bool TabModel::IsOffTheRecord() const {
  return is_off_the_record_;
}

browser_sync::SyncedWindowDelegate* TabModel::GetSyncedWindowDelegate() const {
  return synced_window_delegate_.get();
}

SessionID::id_type TabModel::GetSessionId() const {
  return session_id_.id();
}

void TabModel::BroadcastSessionRestoreComplete() {
  if (profile_) {
    NotificationService::current()->Notify(
        chrome::NOTIFICATION_SESSION_RESTORE_COMPLETE,
        content::Source<Profile>(profile_),
        NotificationService::NoDetails());
  } else {
    // TODO(nyquist): Uncomment this once downstream Android uses new
    // constructor that takes a Profile* argument. See crbug.com/159704.
    // NOTREACHED();
  }
}

ToolbarModel* TabModel::GetToolbarModel() {
  return toolbar_model_.get();
}

ToolbarModel::SecurityLevel TabModel::GetSecurityLevelForCurrentTab() {
  return toolbar_model_->GetSecurityLevel();
}

string16 TabModel::GetSearchTermsForCurrentTab() {
  return toolbar_model_->GetText(true);
}

std::string TabModel::GetQueryExtractionParam() {
  if (!profile_)
    return std::string();
  UIThreadSearchTermsData search_terms_data(profile_);
  return search_terms_data.InstantExtendedEnabledParam();
}

string16 TabModel::GetCorpusNameForCurrentTab() {
  return toolbar_model_->GetCorpusNameForMobile();
}

void TabModel::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_DESTROYED:
      // Our profile just got destroyed, so we delete our pointer to it.
      profile_ = NULL;
      break;
    case chrome::NOTIFICATION_PROFILE_CREATED:
      // Our incognito tab model out lives the profile, so we need to recapture
      // the pointer if ours was previously deleted.
      // NOTIFICATION_PROFILE_DESTROYED is not sent for every destruction, so
      // we overwrite the pointer regardless of whether it's NULL.
      if (is_off_the_record_) {
        Profile* profile = content::Source<Profile>(source).ptr();
        if (profile && profile->IsOffTheRecord())
          profile_ = profile;
      }
      break;
    default:
      NOTREACHED();
  }
}
