// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info.h"

#include "chrome/browser/notifications/sync_notifier/image_holder.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info_service.h"
#include "chrome/browser/profiles/profile.h"
#include "sync/api/sync_data.h"
#include "sync/protocol/synced_notification_app_info_specifics.pb.h"

namespace notifier {

SyncedNotificationAppInfo::SyncedNotificationAppInfo(
    Profile* const profile,
    const std::string& settings_display_name,
    SyncedNotificationAppInfoService* synced_notification_app_info_service)
    : profile_(profile),
      settings_display_name_(settings_display_name),
      synced_notification_app_info_service_(
          synced_notification_app_info_service) {}

SyncedNotificationAppInfo::~SyncedNotificationAppInfo() {}

GURL SyncedNotificationAppInfo::settings_icon_url() {
  if (settings_holder_ != NULL)
    return settings_holder_->low_dpi_url();
  else
    return GURL();
}

bool SyncedNotificationAppInfo::HasAppId(const std::string& app_id) {
  std::vector<std::string>::iterator it;

  for (it = app_ids_.begin(); it != app_ids_.end(); ++it) {
    if (app_id == *it)
      return true;
  }

  return false;
}

void SyncedNotificationAppInfo::AddAppId(const std::string& app_id) {
  if (HasAppId(app_id))
    return;

  app_ids_.push_back(app_id);
}

void SyncedNotificationAppInfo::RemoveAppId(const std::string& app_id) {
  std::vector<std::string>::iterator it;

  for (it = app_ids_.begin(); it != app_ids_.end(); ++it) {
    if (app_id == *it) {
      app_ids_.erase(it);
      return;
    }
  }
}

void SyncedNotificationAppInfo::SetWelcomeLinkUrl(
    const GURL& welcome_link_url) {
  welcome_link_url_ = welcome_link_url;
}

void SyncedNotificationAppInfo::SetSettingsURLs(
    const GURL& settings_low_dpi, const GURL& settings_high_dpi) {
  settings_holder_.reset(new ImageHolder(settings_low_dpi,
                                         settings_high_dpi,
                                         profile_,
                                         this));
}

void SyncedNotificationAppInfo::SetMonochromeURLs(
    const GURL& monochrome_low_dpi, const GURL& monochrome_high_dpi) {
  monochrome_holder_.reset(new ImageHolder(monochrome_low_dpi,
                                           monochrome_high_dpi,
                                           profile_,
                                           this));
}

void SyncedNotificationAppInfo::SetWelcomeURLs(
    const GURL& welcome_low_dpi, const GURL& welcome_high_dpi) {
  welcome_holder_.reset(new ImageHolder(welcome_low_dpi,
                                        welcome_high_dpi,
                                        profile_,
                                        this));
}

gfx::Image SyncedNotificationAppInfo::icon() {
  if (settings_holder_ != NULL)
    return settings_holder_->low_dpi_image();
  else
    return gfx::Image();
}

std::vector<std::string> SyncedNotificationAppInfo::GetAppIdList() {
  std::vector<std::string> app_id_list;
  std::vector<std::string>::iterator it;
  for (it = app_ids_.begin(); it != app_ids_.end(); ++it) {
    app_id_list.push_back(*it);
  }

  return app_id_list;
}

// TODO: rename, queing now happens elsewhere.
// Fill up the queue of bitmaps to fetch.
void SyncedNotificationAppInfo::QueueBitmapFetchJobs() {
  // If there are no bitmaps to fetch, call OnBitmapFetchesDone.
  if (AreAllBitmapsFetched()) {
      synced_notification_app_info_service_->OnBitmapFetchesDone(
          added_app_ids_, removed_app_ids_);
    DVLOG(2) << "AppInfo object with no bitmaps, we should add some. "
             << this->settings_display_name_;
    return;
  }
}

// Start the bitmap fetching.  When it is complete, the callback
// will notify the ChromeNotifierService of the new app info availablity.
void SyncedNotificationAppInfo::StartBitmapFetch() {
  if (settings_holder_.get() != NULL)
    settings_holder_->StartFetch();
  if (monochrome_holder_.get() != NULL)
    monochrome_holder_->StartFetch();
  if (welcome_holder_.get() != NULL)
    welcome_holder_->StartFetch();
}

// Method inherited from ImageHolderDelegate
void SyncedNotificationAppInfo::OnFetchComplete() {
  if (AreAllBitmapsFetched()) {
    if (synced_notification_app_info_service_ != NULL) {
      synced_notification_app_info_service_->OnBitmapFetchesDone(
          added_app_ids_, removed_app_ids_);
    }
  }
}

// Check to see if we have responses for all the bitmaps we got a URL for.
bool SyncedNotificationAppInfo::AreAllBitmapsFetched() {
  bool done =
      (settings_holder_.get() == NULL || settings_holder_->IsFetchingDone()) &&
      (monochrome_holder_.get() == NULL ||
       monochrome_holder_->IsFetchingDone()) &&
      (welcome_holder_.get() == NULL || welcome_holder_->IsFetchingDone());

  return done;
}

message_center::NotifierId SyncedNotificationAppInfo::GetNotifierId() {
  return message_center::NotifierId(
      message_center::NotifierId::SYNCED_NOTIFICATION_SERVICE,
      settings_display_name_);
}

}  // namespace notifier
