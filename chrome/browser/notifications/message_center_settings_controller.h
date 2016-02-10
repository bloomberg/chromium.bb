// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_

#include <stddef.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/app_icon_loader.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "ui/message_center/notifier_settings.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

class Profile;

namespace base {
class CancelableTaskTracker;
}

namespace favicon_base {
struct FaviconImageResult;
}

namespace message_center {
class ProfileNotifierGroup;
}

// The class to bridge between the settings UI of notifiers and the preference
// storage.
class MessageCenterSettingsController
    : public message_center::NotifierSettingsProvider,
      public content::NotificationObserver,
      public ProfileAttributesStorage::Observer,
#if defined(OS_CHROMEOS)
      public user_manager::UserManager::UserSessionStateObserver,
#endif
      public extensions::AppIconLoader::Delegate {
 public:
  explicit MessageCenterSettingsController(ProfileAttributesStorage& storage);
  ~MessageCenterSettingsController() override;

  // Overridden from message_center::NotifierSettingsProvider.
  void AddObserver(message_center::NotifierSettingsObserver* observer) override;
  void RemoveObserver(
      message_center::NotifierSettingsObserver* observer) override;
  size_t GetNotifierGroupCount() const override;
  const message_center::NotifierGroup& GetNotifierGroupAt(
      size_t index) const override;
  bool IsNotifierGroupActiveAt(size_t index) const override;
  void SwitchToNotifierGroup(size_t index) override;
  const message_center::NotifierGroup& GetActiveNotifierGroup() const override;
  void GetNotifierList(
      std::vector<message_center::Notifier*>* notifiers) override;
  void SetNotifierEnabled(const message_center::Notifier& notifier,
                          bool enabled) override;
  void OnNotifierSettingsClosing() override;
  bool NotifierHasAdvancedSettings(
      const message_center::NotifierId& notifier_id) const override;
  void OnNotifierAdvancedSettingsRequested(
      const message_center::NotifierId& notifier_id,
      const std::string* notification_id) override;

#if defined(OS_CHROMEOS)
  // Overridden from user_manager::UserManager::UserSessionStateObserver.
  void ActiveUserChanged(const user_manager::User* active_user) override;
#endif

  // Overridden from extensions::AppIconLoader::Delegate.
  void SetAppImage(const std::string& id, const gfx::ImageSkia& image) override;

 private:
  // Overridden from content::NotificationObserver.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
      const base::string16& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
      const base::string16& old_profile_name) override;
  void OnProfileAuthInfoChanged(const base::FilePath& profile_path) override;

  void OnFaviconLoaded(const GURL& url,
                       const favicon_base::FaviconImageResult& favicon_result);

#if defined(OS_CHROMEOS)
  // Sets up the notifier group for the guest session. This needs to be
  // separated from RebuildNotifierGroup() and called asynchronously to avoid
  // the infinite loop of creating profile. See more the comment of
  // RebuildNotifierGroups().
  void CreateNotifierGroupForGuestLogin();
#endif

  // Sets up the notifier groups for all profiles. If |notify| is true, then
  // it sends out a NotifierGroupChange notification to each observer.
  void RebuildNotifierGroups(bool notify);

  // The views displaying notifier settings.
  base::ObserverList<message_center::NotifierSettingsObserver> observers_;

  // The task tracker for loading favicons.
  scoped_ptr<base::CancelableTaskTracker> favicon_tracker_;

  scoped_ptr<extensions::AppIconLoader> app_icon_loader_;

  std::map<base::string16, ContentSettingsPattern> patterns_;

  // The list of all configurable notifier groups. This is each profile that is
  // loaded (and in the ProfileAttributesStorage - so no incognito profiles go
  // here).
  std::vector<scoped_ptr<message_center::ProfileNotifierGroup>>
      notifier_groups_;

  size_t current_notifier_group_;

  content::NotificationRegistrar registrar_;

  ProfileAttributesStorage& storage_;

  base::WeakPtrFactory<MessageCenterSettingsController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterSettingsController);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_
