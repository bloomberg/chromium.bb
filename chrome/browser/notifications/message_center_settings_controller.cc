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
#include "ui/message_center/notifier_settings_view.h"
#include "ui/views/widget/widget.h"

MessageCenterSettingsController::MessageCenterSettingsController()
    : settings_view_(NULL) {
}

MessageCenterSettingsController::~MessageCenterSettingsController() {
  if (settings_view_) {
    settings_view_->set_delegate(NULL);
    settings_view_->GetWidget()->Close();
    settings_view_ = NULL;
  }
}

void MessageCenterSettingsController::ShowSettingsDialog(
    gfx::NativeView context) {
  if (settings_view_)
    settings_view_->GetWidget()->StackAtTop();
  else
    settings_view_ =
        message_center::NotifierSettingsView::Create(this, context);
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
    // Currently, our notification API is provided for experimental apps.
    // TODO(mukai, miket): determine the actual rule and fix here.
    if (!extension->is_app() || !extension->HasAPIPermission(
            extensions::APIPermission::kExperimental)) {
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
  for (ContentSettingsForOneType::const_iterator iter = settings.begin();
       iter != settings.end(); ++iter) {
    if (iter->primary_pattern == ContentSettingsPattern::Wildcard() &&
        iter->secondary_pattern == ContentSettingsPattern::Wildcard() &&
        iter->source != "preference") {
      continue;
    }

    std::string url_pattern = iter->primary_pattern.ToString();
    GURL url(url_pattern);
    notifiers->push_back(new message_center::Notifier(
        url,
        UTF8ToUTF16(url_pattern),
        notification_service->GetContentSetting(url) == CONTENT_SETTING_ALLOW));
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
        ContentSettingsPattern pattern =
            ContentSettingsPattern::FromURL(notifier.url);
        if (pattern.IsValid())
          notification_service->ClearSetting(pattern);
        else
          LOG(ERROR) << "Invalid url pattern: " << notifier.url.spec();
      }
      break;
    }
  }
}

void MessageCenterSettingsController::OnNotifierSettingsClosing() {
  settings_view_ = NULL;
  DCHECK(favicon_tracker_.get());
  favicon_tracker_->TryCancelAll();
}

void MessageCenterSettingsController::OnFaviconLoaded(
    const GURL& url,
    const history::FaviconImageResult& favicon_result) {
  if (!settings_view_)
    return;
  settings_view_->UpdateFavicon(url, favicon_result.image.AsImageSkia());
}


void MessageCenterSettingsController::SetAppImage(const std::string& id,
                                                  const gfx::ImageSkia& image) {
  if (!settings_view_)
    return;
  settings_view_->UpdateIconImage(id, image);
}
