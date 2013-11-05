// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/message_center_settings_controller.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/i18n/string_compare.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/app_icon_loader_impl.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/extensions/api/notifications.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/favicon/favicon_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "grit/theme_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center_style.h"

#if defined(OS_CHROMEOS)
#include "ash/system/system_notifier.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif

using message_center::Notifier;
using message_center::NotifierId;

namespace message_center {
class ProfileNotifierGroup : public message_center::NotifierGroup {
 public:
  ProfileNotifierGroup(const gfx::Image& icon,
                       const string16& display_name,
                       const string16& login_info,
                       size_t index,
                       const base::FilePath& profile_path);
  virtual ~ProfileNotifierGroup() {}

  Profile* profile() const { return profile_; }

 private:
  Profile* profile_;
};

ProfileNotifierGroup::ProfileNotifierGroup(const gfx::Image& icon,
                                           const string16& display_name,
                                           const string16& login_info,
                                           size_t index,
                                           const base::FilePath& profile_path)
    : message_center::NotifierGroup(icon, display_name, login_info, index),
      profile_(NULL) {
  // Try to get the profile
  profile_ =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
}
}  // namespace message_center

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

MessageCenterSettingsController::MessageCenterSettingsController(
    ProfileInfoCache* profile_info_cache)
    : current_notifier_group_(0), profile_info_cache_(profile_info_cache) {
  DCHECK(profile_info_cache_);
  // The following events all represent changes that may need to be reflected in
  // the profile selector context menu, so listen for them all.  We'll just
  // rebuild the list when we get any of them.
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_ADDED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
                 content::NotificationService::AllBrowserContextsAndSources());
  RebuildNotifierGroups();
}

MessageCenterSettingsController::~MessageCenterSettingsController() {
}

void MessageCenterSettingsController::AddObserver(
    message_center::NotifierSettingsObserver* observer) {
  observers_.AddObserver(observer);
}

void MessageCenterSettingsController::RemoveObserver(
    message_center::NotifierSettingsObserver* observer) {
  observers_.RemoveObserver(observer);
}

size_t MessageCenterSettingsController::GetNotifierGroupCount() const {
  return notifier_groups_.size();
}

const message_center::NotifierGroup&
MessageCenterSettingsController::GetNotifierGroupAt(size_t index) const {
  DCHECK_LT(index, notifier_groups_.size());
  return *(notifier_groups_[index]);
}

bool MessageCenterSettingsController::IsNotifierGroupActiveAt(
    size_t index) const {
  return current_notifier_group_ == index;
}

const message_center::NotifierGroup&
MessageCenterSettingsController::GetActiveNotifierGroup() const {
  DCHECK_LT(current_notifier_group_, notifier_groups_.size());
  return *(notifier_groups_[current_notifier_group_]);
}

void MessageCenterSettingsController::SwitchToNotifierGroup(size_t index) {
  DCHECK_LT(index, notifier_groups_.size());
  if (current_notifier_group_ == index)
    return;

  current_notifier_group_ = index;
  FOR_EACH_OBSERVER(message_center::NotifierSettingsObserver,
                    observers_,
                    NotifierGroupChanged());
}

void MessageCenterSettingsController::GetNotifierList(
    std::vector<Notifier*>* notifiers) {
  DCHECK(notifiers);
  // Temporarily use the last used profile to prevent chrome from crashing when
  // the default profile is not loaded.
  Profile* profile = GetCurrentProfile();
  if (!profile)
    return;

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
       iter != extension_set->end();
       ++iter) {
    const extensions::Extension* extension = iter->get();
    if (!extension->HasAPIPermission(
            extensions::APIPermission::kNotification)) {
      continue;
    }

    NotifierId notifier_id(NotifierId::APPLICATION, extension->id());
    notifiers->push_back(new Notifier(
        notifier_id,
        UTF8ToUTF16(extension->name()),
        notification_service->IsNotifierEnabled(notifier_id)));
    app_icon_loader_->FetchImage(extension->id());
  }

  if (notifier::ChromeNotifierServiceFactory::UseSyncedNotifications(
          CommandLine::ForCurrentProcess())) {
    notifier::ChromeNotifierService* sync_notifier_service =
        notifier::ChromeNotifierServiceFactory::GetInstance()->GetForProfile(
            profile, Profile::EXPLICIT_ACCESS);
    if (sync_notifier_service) {
      sync_notifier_service->GetSyncedNotificationServices(notifiers);

      if (comparator)
        std::sort(notifiers->begin(), notifiers->end(), *comparator);
      else
        std::sort(notifiers->begin(), notifiers->end(), SimpleCompareNotifiers);
    }
  }

  int app_count = notifiers->size();

  ContentSettingsForOneType settings;
  notification_service->GetNotificationsSettings(&settings);
  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile, Profile::EXPLICIT_ACCESS);
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
    NotifierId notifier_id(url);
    notifiers->push_back(new Notifier(
        notifier_id,
        name,
        notification_service->IsNotifierEnabled(notifier_id)));
    patterns_[name] = iter->primary_pattern;
    FaviconService::FaviconForURLParams favicon_params(
        url,
        chrome::FAVICON | chrome::TOUCH_ICON,
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

  // Screenshot notification feature is only for ChromeOS. See crbug.com/238358
#if defined(OS_CHROMEOS)
  const string16 screenshot_name =
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_NOTIFIER_SCREENSHOT_NAME);
  NotifierId screenshot_notifier_id(ash::system_notifier::NOTIFIER_SCREENSHOT);
  Notifier* const screenshot_notifier = new Notifier(
      screenshot_notifier_id,
      screenshot_name,
      notification_service->IsNotifierEnabled(screenshot_notifier_id));
  screenshot_notifier->icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_SCREENSHOT_NOTIFICATION_ICON);
  notifiers->push_back(screenshot_notifier);
#endif

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
  Profile* profile = GetCurrentProfile();
  DCHECK(profile);

  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile);

  if (notifier.notifier_id.type == NotifierId::WEB_PAGE) {
    // WEB_PAGE notifier cannot handle in DesktopNotificationService
    // since it has the exact URL pattern.
    // TODO(mukai): fix this.
    ContentSetting default_setting =
        notification_service->GetDefaultContentSetting(NULL);
    DCHECK(default_setting == CONTENT_SETTING_ALLOW ||
           default_setting == CONTENT_SETTING_BLOCK ||
           default_setting == CONTENT_SETTING_ASK);
    if ((enabled && default_setting != CONTENT_SETTING_ALLOW) ||
        (!enabled && default_setting == CONTENT_SETTING_ALLOW)) {
      if (notifier.notifier_id.url.is_valid()) {
        if (enabled)
          notification_service->GrantPermission(notifier.notifier_id.url);
        else
          notification_service->DenyPermission(notifier.notifier_id.url);
      } else {
        LOG(ERROR) << "Invalid url pattern: "
                   << notifier.notifier_id.url.spec();
      }
    } else {
      std::map<string16, ContentSettingsPattern>::const_iterator iter =
          patterns_.find(notifier.name);
      if (iter != patterns_.end()) {
        notification_service->ClearSetting(iter->second);
      } else {
        LOG(ERROR) << "Invalid url pattern: "
                   << notifier.notifier_id.url.spec();
      }
    }
  } else {
    notification_service->SetNotifierEnabled(notifier.notifier_id, enabled);
    if (notifier.notifier_id.type == NotifierId::SYNCED_NOTIFICATION_SERVICE) {
      notifier::ChromeNotifierService* notifier_service =
          notifier::ChromeNotifierServiceFactory::GetInstance()->GetForProfile(
              profile, Profile::EXPLICIT_ACCESS);
      notifier_service->OnSyncedNotificationServiceEnabled(
          notifier.notifier_id.id, enabled);
    }
  }
}

void MessageCenterSettingsController::OnNotifierSettingsClosing() {
  DCHECK(favicon_tracker_.get());
  favicon_tracker_->TryCancelAll();
  patterns_.clear();
}

bool MessageCenterSettingsController::NotifierHasAdvancedSettings(
    const NotifierId& notifier_id) const {
  // TODO(dewittj): Refactor this so that notifiers have a delegate that can
  // handle this in a more appropriate location.
  if (notifier_id.type != NotifierId::APPLICATION)
    return false;

  const std::string& extension_id = notifier_id.id;

  Profile* profile = GetCurrentProfile();
  if (!profile)
    return false;

  extensions::EventRouter* event_router =
      extensions::ExtensionSystem::Get(profile)->event_router();

  return event_router->ExtensionHasEventListener(
      extension_id, extensions::api::notifications::OnShowSettings::kEventName);
}

void MessageCenterSettingsController::OnNotifierAdvancedSettingsRequested(
    const NotifierId& notifier_id,
    const std::string* notification_id) {
  // TODO(dewittj): Refactor this so that notifiers have a delegate that can
  // handle this in a more appropriate location.
  if (notifier_id.type != NotifierId::APPLICATION)
    return;

  const std::string& extension_id = notifier_id.id;

  Profile* profile = GetCurrentProfile();
  if (!profile)
    return;

  extensions::EventRouter* event_router =
      extensions::ExtensionSystem::Get(profile)->event_router();
  scoped_ptr<base::ListValue> args(new base::ListValue());

  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::api::notifications::OnShowSettings::kEventName, args.Pass()));
  event_router->DispatchEventToExtension(extension_id, event.Pass());
}

void MessageCenterSettingsController::OnFaviconLoaded(
    const GURL& url,
    const chrome::FaviconImageResult& favicon_result) {
  FOR_EACH_OBSERVER(message_center::NotifierSettingsObserver,
                    observers_,
                    UpdateIconImage(NotifierId(url), favicon_result.image));
}


void MessageCenterSettingsController::SetAppImage(const std::string& id,
                                                  const gfx::ImageSkia& image) {
  FOR_EACH_OBSERVER(message_center::NotifierSettingsObserver,
                    observers_,
                    UpdateIconImage(NotifierId(NotifierId::APPLICATION, id),
                                    gfx::Image(image)));
}

void MessageCenterSettingsController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  RebuildNotifierGroups();
  FOR_EACH_OBSERVER(message_center::NotifierSettingsObserver,
                    observers_,
                    NotifierGroupChanged());
}

Profile* MessageCenterSettingsController::GetCurrentProfile() const {
  if (notifier_groups_.size() > current_notifier_group_)
    return notifier_groups_[current_notifier_group_]->profile();

#if defined(OS_CHROMEOS)
  return ProfileManager::GetDefaultProfileOrOffTheRecord();
#else
  NOTREACHED();
  return NULL;
#endif
}

void MessageCenterSettingsController::RebuildNotifierGroups() {
  notifier_groups_.clear();
  current_notifier_group_ = 0;

  const size_t count = profile_info_cache_->GetNumberOfProfiles();

  for (size_t i = 0; i < count; ++i) {
    scoped_ptr<message_center::ProfileNotifierGroup> group(
        new message_center::ProfileNotifierGroup(
            profile_info_cache_->GetAvatarIconOfProfileAtIndex(i),
            profile_info_cache_->GetNameOfProfileAtIndex(i),
            profile_info_cache_->GetUserNameOfProfileAtIndex(i),
            i,
            profile_info_cache_->GetPathOfProfileAtIndex(i)));
    if (group->profile() == NULL)
      continue;

#if defined(OS_CHROMEOS)
    // In ChromeOS, the login screen first creates a dummy profile which is not
    // actually used, and then the real profile for the user is created when
    // login (or turns into kiosk mode). This profile should be skipped.
    if (chromeos::ProfileHelper::IsSigninProfile(group->profile()))
      continue;
#endif
    notifier_groups_.push_back(group.release());
  }
}
