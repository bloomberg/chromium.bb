// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/message_center_settings_controller.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/app_icon_loader_impl.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center_constants.h"

MessageCenterSettingsController::MessageCenterSettingsController()
    : delegate_(NULL) {
}

MessageCenterSettingsController::~MessageCenterSettingsController() {
}

void MessageCenterSettingsController::ShowSettingsDialog(
    gfx::NativeView context) {
  delegate_ = message_center::ShowSettings(this, context);
}

void MessageCenterSettingsController::GetNotifierList(
    std::vector<message_center::Notifier*>* notifiers) {
  DCHECK(notifiers);
  // TODO(mukai): Fix this for multi-profile.
  Profile* profile = ProfileManager::GetDefaultProfile();
  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile);

  ExtensionService* extension_service = profile->GetExtensionService();
  const ExtensionSet* extension_set = extension_service->extensions();
  app_icon_loader_.reset(new extensions::AppIconLoaderImpl(
      profile, message_center::kSettingsIconSize, this));
  for (ExtensionSet::const_iterator iter = extension_set->begin();
       iter != extension_set->end(); ++iter) {
    const extensions::Extension* extension = *iter;
    if (!extension->HasAPIPermission(
      extensions::APIPermission::kNotification)) {
      continue;
    }

    notifiers->push_back(new message_center::Notifier(
        extension->id(),
        UTF8ToUTF16(extension->name()),
        notification_service->IsExtensionEnabled(extension->id())));
    app_icon_loader_->FetchImage(extension->id());
  }

  ContentSettingsForOneType settings;
  notification_service->GetNotificationsSettings(&settings);
  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);
  favicon_tracker_.reset(new CancelableTaskTracker());
  patterns_.clear();
  for (ContentSettingsForOneType::const_iterator iter = settings.begin();
       iter != settings.end(); ++iter) {
    if (iter->primary_pattern == ContentSettingsPattern::Wildcard() &&
        iter->secondary_pattern == ContentSettingsPattern::Wildcard() &&
        iter->source != "preference") {
      continue;
    }

    std::string url_pattern = iter->primary_pattern.ToString();
    string16 name = UTF8ToUTF16(url_pattern);
    GURL url(url_pattern);
    notifiers->push_back(new message_center::Notifier(
        url,
        name,
        notification_service->GetContentSetting(url) == CONTENT_SETTING_ALLOW));
    patterns_[name] = iter->primary_pattern;
    FaviconService::FaviconForURLParams favicon_params(
        profile, url, history::FAVICON | history::TOUCH_ICON,
        message_center::kSettingsIconSize);
    // Note that favicon service obtains the favicon from history. This means
    // that it will fail to obtain the image if there are no history data for
    // that URL.
    favicon_service->GetFaviconImageForURL(
        favicon_params,
        base::Bind(&MessageCenterSettingsController::OnFaviconLoaded,
                   base::Unretained(this), url),
        favicon_tracker_.get());
  }
}

void MessageCenterSettingsController::SetNotifierEnabled(
    const message_center::Notifier& notifier,
    bool enabled) {
  // TODO(mukai): Fix this for multi-profile.
  Profile* profile = ProfileManager::GetDefaultProfile();
  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile);

  switch (notifier.type) {
    case message_center::Notifier::APPLICATION:
      notification_service->SetExtensionEnabled(notifier.id, enabled);
      break;
    case message_center::Notifier::WEB_PAGE: {
      ContentSetting default_setting =
          notification_service->GetDefaultContentSetting(NULL);
      DCHECK(default_setting == CONTENT_SETTING_ALLOW ||
             default_setting == CONTENT_SETTING_BLOCK ||
             default_setting == CONTENT_SETTING_ASK);
      if ((enabled && default_setting != CONTENT_SETTING_ALLOW) ||
          (!enabled && default_setting == CONTENT_SETTING_ALLOW)) {
        if (notifier.url.is_valid()) {
          if (enabled)
            notification_service->GrantPermission(notifier.url);
          else
            notification_service->DenyPermission(notifier.url);
        } else {
          LOG(ERROR) << "Invalid url pattern: " << notifier.url.spec();
        }
      } else {
        std::map<string16, ContentSettingsPattern>::const_iterator iter =
            patterns_.find(notifier.name);
        if (iter != patterns_.end()) {
          notification_service->ClearSetting(iter->second);
        } else {
          LOG(ERROR) << "Invalid url pattern: " << notifier.url.spec();
        }
      }
      break;
    }
  }
}

void MessageCenterSettingsController::OnNotifierSettingsClosing() {
  delegate_ = NULL;
  DCHECK(favicon_tracker_.get());
  favicon_tracker_->TryCancelAll();
  patterns_.clear();
}

void MessageCenterSettingsController::OnFaviconLoaded(
    const GURL& url,
    const history::FaviconImageResult& favicon_result) {
  if (!delegate_)
    return;
  delegate_->UpdateFavicon(url, favicon_result.image);
}


void MessageCenterSettingsController::SetAppImage(const std::string& id,
                                                  const gfx::ImageSkia& image) {
  if (!delegate_)
    return;
  delegate_->UpdateIconImage(id, gfx::Image(image) );
}
