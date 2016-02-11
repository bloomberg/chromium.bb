// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/message_center_settings_controller.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/i18n/string_compare.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/app_icon_loader_impl.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/notifications.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center_style.h"
#include "ui/strings/grit/ui_strings.h"

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
                       const base::string16& display_name,
                       const base::string16& login_info,
                       const base::FilePath& profile_path);
  ProfileNotifierGroup(const gfx::Image& icon,
                       const base::string16& display_name,
                       const base::string16& login_info,
                       Profile* profile);
  virtual ~ProfileNotifierGroup() {}

  Profile* profile() const { return profile_; }

 private:
  Profile* profile_;
};

ProfileNotifierGroup::ProfileNotifierGroup(const gfx::Image& icon,
                                           const base::string16& display_name,
                                           const base::string16& login_info,
                                           const base::FilePath& profile_path)
    : message_center::NotifierGroup(icon, display_name, login_info),
      profile_(NULL) {
  // Try to get the profile
  profile_ =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
}

ProfileNotifierGroup::ProfileNotifierGroup(const gfx::Image& icon,
                                           const base::string16& display_name,
                                           const base::string16& login_info,
                                           Profile* profile)
    : message_center::NotifierGroup(icon, display_name, login_info),
      profile_(profile) {
}

}  // namespace message_center

namespace {
class NotifierComparator {
 public:
  explicit NotifierComparator(icu::Collator* collator) : collator_(collator) {}

  bool operator() (Notifier* n1, Notifier* n2) {
    if (n1->notifier_id.type != n2->notifier_id.type)
      return n1->notifier_id.type < n2->notifier_id.type;

    if (collator_) {
      return base::i18n::CompareString16WithCollator(*collator_, n1->name,
                                                     n2->name) == UCOL_LESS;
    }
    return n1->name < n2->name;
  }

 private:
  icu::Collator* collator_;
};

}  // namespace

MessageCenterSettingsController::MessageCenterSettingsController(
    ProfileAttributesStorage& profile_attributes_storage)
    : current_notifier_group_(0),
      profile_attributes_storage_(profile_attributes_storage),
      weak_factory_(this) {
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
  profile_attributes_storage_.AddObserver(this);
  RebuildNotifierGroups(false);

#if defined(OS_CHROMEOS)
  // UserManager may not exist in some tests.
  if (user_manager::UserManager::IsInitialized())
    user_manager::UserManager::Get()->AddSessionStateObserver(this);
#endif
}

MessageCenterSettingsController::~MessageCenterSettingsController() {
  profile_attributes_storage_.RemoveObserver(this);
#if defined(OS_CHROMEOS)
  // UserManager may not exist in some tests.
  if (user_manager::UserManager::IsInitialized())
    user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
#endif
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
  if (notifier_groups_.size() <= current_notifier_group_)
    return;
  // Temporarily use the last used profile to prevent chrome from crashing when
  // the default profile is not loaded.
  Profile* profile = notifier_groups_[current_notifier_group_]->profile();

  NotifierStateTracker* notifier_state_tracker =
      NotifierStateTrackerFactory::GetForProfile(profile);

  const extensions::ExtensionSet& extension_set =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  // The extension icon size has to be 32x32 at least to load bigger icons if
  // the icon doesn't exist for the specified size, and in that case it falls
  // back to the default icon. The fetched icon will be resized in the settings
  // dialog. See chrome/browser/extensions/extension_icon_image.cc and
  // crbug.com/222931
  app_icon_loader_.reset(new extensions::AppIconLoaderImpl(
      profile, extension_misc::EXTENSION_ICON_SMALL, this));
  for (extensions::ExtensionSet::const_iterator iter = extension_set.begin();
       iter != extension_set.end();
       ++iter) {
    const extensions::Extension* extension = iter->get();
    if (!extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kNotifications)) {
      continue;
    }

    // Hosted apps are no longer able to affect the notifications permission
    // state for web notifications.
    // TODO(dewittj): Deprecate the 'notifications' permission for hosted apps.
    if (extension->is_hosted_app())
      continue;

    NotifierId notifier_id(NotifierId::APPLICATION, extension->id());
    notifiers->push_back(new Notifier(
        notifier_id,
        base::UTF8ToUTF16(extension->name()),
        notifier_state_tracker->IsNotifierEnabled(notifier_id)));
    app_icon_loader_->FetchImage(extension->id());
  }

  ContentSettingsForOneType settings;
  DesktopNotificationProfileUtil::GetNotificationsSettings(profile, &settings);

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  favicon_tracker_.reset(new base::CancelableTaskTracker());
  patterns_.clear();
  for (ContentSettingsForOneType::const_iterator iter = settings.begin();
       iter != settings.end(); ++iter) {
    if (iter->primary_pattern == ContentSettingsPattern::Wildcard() &&
        iter->secondary_pattern == ContentSettingsPattern::Wildcard() &&
        iter->source != "preference") {
      continue;
    }

    std::string url_pattern = iter->primary_pattern.ToString();
    base::string16 name = base::UTF8ToUTF16(url_pattern);
    GURL url(url_pattern);
    NotifierId notifier_id(url);
    notifiers->push_back(new Notifier(
        notifier_id,
        name,
        notifier_state_tracker->IsNotifierEnabled(notifier_id)));
    patterns_[name] = iter->primary_pattern;
    // Note that favicon service obtains the favicon from history. This means
    // that it will fail to obtain the image if there are no history data for
    // that URL.
    favicon_service->GetFaviconImageForPageURL(
        url,
        base::Bind(&MessageCenterSettingsController::OnFaviconLoaded,
                   base::Unretained(this),
                   url),
        favicon_tracker_.get());
  }

  // Screenshot notification feature is only for ChromeOS. See crbug.com/238358
#if defined(OS_CHROMEOS)
  const base::string16 screenshot_name =
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_NOTIFIER_SCREENSHOT_NAME);
  NotifierId screenshot_notifier_id(
      NotifierId::SYSTEM_COMPONENT, ash::system_notifier::kNotifierScreenshot);
  Notifier* const screenshot_notifier = new Notifier(
      screenshot_notifier_id,
      screenshot_name,
      notifier_state_tracker->IsNotifierEnabled(screenshot_notifier_id));
  screenshot_notifier->icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_SCREENSHOT_NOTIFICATION_ICON);
  notifiers->push_back(screenshot_notifier);
#endif

  UErrorCode error = U_ZERO_ERROR;
  scoped_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  scoped_ptr<NotifierComparator> comparator(
      new NotifierComparator(U_SUCCESS(error) ? collator.get() : NULL));

  std::sort(notifiers->begin(), notifiers->end(), *comparator);
}

void MessageCenterSettingsController::SetNotifierEnabled(
    const Notifier& notifier,
    bool enabled) {
  DCHECK_LT(current_notifier_group_, notifier_groups_.size());
  Profile* profile = notifier_groups_[current_notifier_group_]->profile();

  if (notifier.notifier_id.type == NotifierId::WEB_PAGE) {
    // WEB_PAGE notifier cannot handle in DesktopNotificationService
    // since it has the exact URL pattern.
    // TODO(mukai): fix this.
    ContentSetting default_setting =
        HostContentSettingsMapFactory::GetForProfile(profile)
          ->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, NULL);

    DCHECK(default_setting == CONTENT_SETTING_ALLOW ||
           default_setting == CONTENT_SETTING_BLOCK ||
           default_setting == CONTENT_SETTING_ASK);

    // The content setting for notifications needs to clear when it changes to
    // the default value or get explicitly set when it differs from the default.
    bool differs_from_default_value =
        (default_setting != CONTENT_SETTING_ALLOW && enabled) ||
        (default_setting == CONTENT_SETTING_ALLOW && !enabled);

    if (differs_from_default_value) {
      if (notifier.notifier_id.url.is_valid()) {
        if (enabled) {
          DesktopNotificationProfileUtil::GrantPermission(
              profile, notifier.notifier_id.url);
        } else {
          DesktopNotificationProfileUtil::DenyPermission(
              profile, notifier.notifier_id.url);
        }
      } else {
        LOG(ERROR) << "Invalid url pattern: "
                   << notifier.notifier_id.url.spec();
      }
    } else {
      ContentSettingsPattern pattern;

      const auto& iter = patterns_.find(notifier.name);
      if (iter != patterns_.end()) {
        pattern = iter->second;
      } else if (notifier.notifier_id.url.is_valid()) {
        pattern =
            ContentSettingsPattern::FromURLNoWildcard(notifier.notifier_id.url);
      } else {
        LOG(ERROR) << "Invalid url pattern: "
                   << notifier.notifier_id.url.spec();
      }

      if (pattern.IsValid())
        DesktopNotificationProfileUtil::ClearSetting(profile, pattern);
    }
  } else {
    NotifierStateTrackerFactory::GetForProfile(profile)
        ->SetNotifierEnabled(notifier.notifier_id, enabled);
  }
  FOR_EACH_OBSERVER(message_center::NotifierSettingsObserver,
                    observers_,
                    NotifierEnabledChanged(notifier.notifier_id, enabled));
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

  if (notifier_groups_.size() < current_notifier_group_)
    return false;
  Profile* profile = notifier_groups_[current_notifier_group_]->profile();

  extensions::EventRouter* event_router = extensions::EventRouter::Get(profile);

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

  if (notifier_groups_.size() < current_notifier_group_)
    return;
  Profile* profile = notifier_groups_[current_notifier_group_]->profile();

  extensions::EventRouter* event_router = extensions::EventRouter::Get(profile);
  scoped_ptr<base::ListValue> args(new base::ListValue());

  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::NOTIFICATIONS_ON_SHOW_SETTINGS,
      extensions::api::notifications::OnShowSettings::kEventName,
      std::move(args)));
  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

void MessageCenterSettingsController::OnFaviconLoaded(
    const GURL& url,
    const favicon_base::FaviconImageResult& favicon_result) {
  FOR_EACH_OBSERVER(message_center::NotifierSettingsObserver,
                    observers_,
                    UpdateIconImage(NotifierId(url), favicon_result.image));
}


#if defined(OS_CHROMEOS)
void MessageCenterSettingsController::ActiveUserChanged(
    const user_manager::User* active_user) {
  RebuildNotifierGroups(false);
}
#endif

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
  // GetOffTheRecordProfile() may create a new off-the-record profile, but that
  // doesn't need to rebuild the groups.
  if (type == chrome::NOTIFICATION_PROFILE_CREATED &&
      content::Source<Profile>(source).ptr()->IsOffTheRecord()) {
    return;
  }

  RebuildNotifierGroups(true);
}

void MessageCenterSettingsController::OnProfileAdded(
    const base::FilePath& profile_path) {
  RebuildNotifierGroups(true);
}
void MessageCenterSettingsController::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  RebuildNotifierGroups(true);
}
void MessageCenterSettingsController::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  RebuildNotifierGroups(true);
}
void MessageCenterSettingsController::OnProfileAuthInfoChanged(
    const base::FilePath& profile_path) {
  RebuildNotifierGroups(true);
}

#if defined(OS_CHROMEOS)
void MessageCenterSettingsController::CreateNotifierGroupForGuestLogin() {
  // Already created.
  if (!notifier_groups_.empty())
    return;

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  // |notifier_groups_| can be empty in login screen too.
  if (!user_manager->IsLoggedInAsGuest())
    return;

  user_manager::User* user = user_manager->GetActiveUser();
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(user);
  DCHECK(profile);

  scoped_ptr<message_center::ProfileNotifierGroup> group(
      new message_center::ProfileNotifierGroup(gfx::Image(user->GetImage()),
                                               user->GetDisplayName(),
                                               user->GetDisplayName(),
                                               profile));

  notifier_groups_.push_back(std::move(group));

  FOR_EACH_OBSERVER(message_center::NotifierSettingsObserver,
                    observers_,
                    NotifierGroupChanged());
}
#endif

void MessageCenterSettingsController::RebuildNotifierGroups(bool notify) {
  notifier_groups_.clear();
  current_notifier_group_ = 0;

  std::vector<ProfileAttributesEntry*> entries =
      profile_attributes_storage_.GetAllProfilesAttributes();
  for (const auto entry : entries) {
    scoped_ptr<message_center::ProfileNotifierGroup> group(
        new message_center::ProfileNotifierGroup(
            entry->GetAvatarIcon(),
            entry->GetName(),
            entry->GetUserName(),
            entry->GetPath()));
    if (group->profile() == NULL)
      continue;

#if defined(OS_CHROMEOS)
    // Allows the active user only.
    // UserManager may not exist in some tests.
    if (user_manager::UserManager::IsInitialized()) {
      user_manager::UserManager* user_manager =
          user_manager::UserManager::Get();
      if (chromeos::ProfileHelper::Get()->GetUserByProfile(group->profile()) !=
          user_manager->GetActiveUser()) {
        continue;
      }
    }

    // In ChromeOS, the login screen first creates a dummy profile which is not
    // actually used, and then the real profile for the user is created when
    // login (or turns into kiosk mode). This profile should be skipped.
    if (chromeos::ProfileHelper::IsSigninProfile(group->profile()))
      continue;
#endif

    notifier_groups_.push_back(std::move(group));
  }

#if defined(OS_CHROMEOS)
  // ChromeOS guest login cannot get the profile from the for-loop above, so
  // get the group here.
  if (notifier_groups_.empty() && user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->IsLoggedInAsGuest()) {
    // Do not invoke CreateNotifierGroupForGuestLogin() directly. In some tests,
    // this method may be called before the primary profile is created, which
    // means ProfileHelper::Get()->GetProfileByUser() will create a new primary
    // profile. But creating a primary profile causes an Observe() before
    // registering it as the primary one, which causes this method which causes
    // another creating a primary profile, and causes an infinite loop.
    // Thus, it would be better to delay creating group for guest login.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(
            &MessageCenterSettingsController::CreateNotifierGroupForGuestLogin,
            weak_factory_.GetWeakPtr()));
  }
#endif

  if (notify) {
    FOR_EACH_OBSERVER(message_center::NotifierSettingsObserver,
                      observers_,
                      NotifierGroupChanged());
  }
}
