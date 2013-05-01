// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/message_center_settings_controller.h"

#include <algorithm>

#include "base/i18n/string_compare.h"
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
#include "chrome/common/extensions/extension_constants.h"
#include "grit/theme_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center_constants.h"

using message_center::Notifier;

namespace {

class NotifierComparator {
 public:
  explicit NotifierComparator(icu::Collator* collator) : collator_(collator) {}

  bool operator() (Notifier* n1, Notifier* n2) {
    return base::i18n::CompareString16WithCollator(
        collator_, n1->name, n2->name) == UCOL_LESS;
  }

 private:
  icu::Collator* collator_;
};

bool SimpleCompareNotifiers(Notifier* n1, Notifier* n2) {
  return n1->name < n2->name;
}

}  // namespace

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
    std::vector<Notifier*>* notifiers) {
  DCHECK(notifiers);
  // TODO(mukai): Fix this for multi-profile.
  Profile* profile = ProfileManager::GetDefaultProfile();
  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile);

  UErrorCode error;
  scoped_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  scoped_ptr<NotifierComparator> comparator;
  if (!U_FAILURE(error))
    comparator.reset(new NotifierComparator(collator.get()));

  ExtensionService* extension_service = profile->GetExtensionService();
  const ExtensionSet* extension_set = extension_service->extensions();
  // The extension icon size has to be 32x32 at least to load bigger icons if
  // the icon doesn't exist for the specified size, and in that case it falls
  // back to the default icon. The fetched icon will be resized in the settings
  // dialog. See chrome/browser/extensions/extension_icon_image.cc and
  // crbug.com/222931
  app_icon_loader_.reset(new extensions::AppIconLoaderImpl(
      profile, extension_misc::EXTENSION_ICON_SMALL, this));
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
  if (comparator)
    std::sort(notifiers->begin(), notifiers->end(), *comparator);
  else
    std::sort(notifiers->begin(), notifiers->end(), SimpleCompareNotifiers);
  int app_count = notifiers->size();

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
  const string16 screenshot_name =
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_NOTIFIER_SCREENSHOT_NAME);
  message_center::Notifier* const screenshot_notifier =
      new message_center::Notifier(
          message_center::Notifier::SCREENSHOT,
          screenshot_name,
          notification_service->IsSystemComponentEnabled(
              message_center::Notifier::SCREENSHOT));
  screenshot_notifier->icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_SCREENSHOT_NOTIFICATION_ICON);
  notifiers->push_back(screenshot_notifier);
  if (comparator) {
    std::sort(notifiers->begin() + app_count, notifiers->end(), *comparator);
  } else {
    std::sort(notifiers->begin() + app_count, notifiers->end(),
              SimpleCompareNotifiers);
  }
}

void MessageCenterSettingsController::SetNotifierEnabled(
    const Notifier& notifier,
    bool enabled) {
  // TODO(mukai): Fix this for multi-profile.
  Profile* profile = ProfileManager::GetDefaultProfile();
  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile);

  switch (notifier.type) {
    case Notifier::APPLICATION:
      notification_service->SetExtensionEnabled(notifier.id, enabled);
      break;
    case Notifier::WEB_PAGE: {
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
    case message_center::Notifier::SYSTEM_COMPONENT:
      notification_service->SetSystemComponentEnabled(
          notifier.system_component_type, enabled);
      break;
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
