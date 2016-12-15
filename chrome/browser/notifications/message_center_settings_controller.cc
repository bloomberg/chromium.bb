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
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/notifications/application_notifier_source.h"
#include "chrome/browser/notifications/web_page_notifier_source.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/notifications.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/event_router.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center_style.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/arc_application_notifier_source_chromeos.h"
#include "chrome/browser/notifications/system_component_notifier_source_chromeos.h"
#endif

using message_center::Notifier;
using message_center::NotifierId;

namespace message_center {

class ProfileNotifierGroup : public message_center::NotifierGroup {
 public:
  ProfileNotifierGroup(const base::string16& display_name,
                       const base::string16& login_info,
                       const base::FilePath& profile_path);
  ProfileNotifierGroup(const base::string16& display_name,
                       const base::string16& login_info,
                       Profile* profile);
  virtual ~ProfileNotifierGroup() {}

  Profile* profile() const { return profile_; }

 private:
  Profile* profile_;
};

ProfileNotifierGroup::ProfileNotifierGroup(const base::string16& display_name,
                                           const base::string16& login_info,
                                           const base::FilePath& profile_path)
    : message_center::NotifierGroup(display_name, login_info), profile_(NULL) {
  // Try to get the profile
  profile_ =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
}

ProfileNotifierGroup::ProfileNotifierGroup(const base::string16& display_name,
                                           const base::string16& login_info,
                                           Profile* profile)
    : message_center::NotifierGroup(display_name, login_info),
      profile_(profile) {}

}  // namespace message_center

namespace {

class NotifierComparator {
 public:
  explicit NotifierComparator(icu::Collator* collator) : collator_(collator) {}

  bool operator()(const std::unique_ptr<Notifier>& n1,
                  const std::unique_ptr<Notifier>& n2) {
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

  sources_.insert(std::make_pair(
      NotifierId::APPLICATION,
      std::unique_ptr<NotifierSource>(new ApplicationNotifierSource(this))));
  sources_.insert(std::make_pair(
      NotifierId::WEB_PAGE,
      std::unique_ptr<NotifierSource>(new WebPageNotifiereSource(this))));

#if defined(OS_CHROMEOS)
  // UserManager may not exist in some tests.
  if (user_manager::UserManager::IsInitialized())
    user_manager::UserManager::Get()->AddSessionStateObserver(this);
  // For system components.
  sources_.insert(
      std::make_pair(NotifierId::SYSTEM_COMPONENT,
                     std::unique_ptr<NotifierSource>(
                         new SystemComponentNotifierSourceChromeOS(this))));
  sources_.insert(
      std::make_pair(NotifierId::ARC_APPLICATION,
                     std::unique_ptr<NotifierSource>(
                         new arc::ArcApplicationNotifierSourceChromeOS(this))));
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
  DispatchNotifierGroupChanged();
}

void MessageCenterSettingsController::GetNotifierList(
    std::vector<std::unique_ptr<Notifier>>* notifiers) {
  DCHECK(notifiers);
  if (notifier_groups_.size() <= current_notifier_group_)
    return;
  // Temporarily use the last used profile to prevent chrome from crashing when
  // the default profile is not loaded.
  Profile* profile = notifier_groups_[current_notifier_group_]->profile();

  for (auto& source : sources_) {
    auto source_notifiers = source.second->GetNotifierList(profile);
    for (auto& notifier : source_notifiers) {
      notifiers->push_back(std::move(notifier));
    }
  }

  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  NotifierComparator comparator(U_SUCCESS(error) ? collator.get() : NULL);

  std::sort(notifiers->begin(), notifiers->end(), comparator);
}

void MessageCenterSettingsController::SetNotifierEnabled(
    const Notifier& notifier,
    bool enabled) {
  DCHECK_LT(current_notifier_group_, notifier_groups_.size());
  Profile* profile = notifier_groups_[current_notifier_group_]->profile();

  if (!sources_.count(notifier.notifier_id.type)) {
    NOTREACHED();
    return;
  }

  sources_[notifier.notifier_id.type]->SetNotifierEnabled(profile, notifier,
                                                          enabled);
}

void MessageCenterSettingsController::OnNotifierSettingsClosing() {
  for (auto& source : sources_) {
    source.second->OnNotifierSettingsClosing();
  }
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
  std::unique_ptr<base::ListValue> args(new base::ListValue());

  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::NOTIFICATIONS_ON_SHOW_SETTINGS,
      extensions::api::notifications::OnShowSettings::kEventName,
      std::move(args)));
  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

#if defined(OS_CHROMEOS)
void MessageCenterSettingsController::ActiveUserChanged(
    const user_manager::User* active_user) {
  RebuildNotifierGroups(false);
}
#endif

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

void MessageCenterSettingsController::OnIconImageUpdated(
    const message_center::NotifierId& id,
    const gfx::Image& image) {
  for (message_center::NotifierSettingsObserver& observer : observers_)
    observer.UpdateIconImage(id, image);
}

void MessageCenterSettingsController::OnNotifierEnabledChanged(
    const message_center::NotifierId& id,
    bool enabled) {
  for (message_center::NotifierSettingsObserver& observer : observers_)
    observer.NotifierEnabledChanged(id, enabled);
}

void MessageCenterSettingsController::DispatchNotifierGroupChanged() {
  for (message_center::NotifierSettingsObserver& observer : observers_)
    observer.NotifierGroupChanged();
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

  std::unique_ptr<message_center::ProfileNotifierGroup> group(
      new message_center::ProfileNotifierGroup(
          user->GetDisplayName(), user->GetDisplayName(), profile));

  notifier_groups_.push_back(std::move(group));
  DispatchNotifierGroupChanged();
}
#endif

void MessageCenterSettingsController::RebuildNotifierGroups(bool notify) {
  notifier_groups_.clear();
  current_notifier_group_ = 0;

  std::vector<ProfileAttributesEntry*> entries =
      profile_attributes_storage_.GetAllProfilesAttributes();
  for (auto* entry : entries) {
    std::unique_ptr<message_center::ProfileNotifierGroup> group(
        new message_center::ProfileNotifierGroup(
            entry->GetName(), entry->GetUserName(), entry->GetPath()));
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

  if (notify)
    DispatchNotifierGroupChanged();
}
