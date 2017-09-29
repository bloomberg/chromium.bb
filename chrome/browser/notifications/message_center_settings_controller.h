// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notifier_controller.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/favicon_base/favicon_types.h"
#include "ui/message_center/notifier_settings.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

class NotifierController;

namespace message_center {
class ProfileNotifierGroup;
}

// The class to bridge between the settings UI of notifiers and the preference
// storage.
class MessageCenterSettingsController
    : public message_center::NotifierSettingsProvider,
      public ProfileAttributesStorage::Observer,
#if defined(OS_CHROMEOS)
      public user_manager::UserManager::UserSessionStateObserver,
#endif
      public NotifierController::Observer {
 public:
  explicit MessageCenterSettingsController(
      ProfileAttributesStorage& profile_attributes_storage);
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
  void GetNotifierList(std::vector<std::unique_ptr<message_center::Notifier>>*
                           notifiers) override;
  void SetNotifierEnabled(const message_center::NotifierId& notifier_id,
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

 private:
  // Overridden from ProfileAttributesStorage::Observer.
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
      const base::string16& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
      const base::string16& old_profile_name) override;
  void OnProfileAuthInfoChanged(const base::FilePath& profile_path) override;

  // Overridden from NotifierController::Observer.
  void OnIconImageUpdated(const message_center::NotifierId&,
                          const gfx::Image&) override;
  void OnNotifierEnabledChanged(const message_center::NotifierId&,
                                bool) override;

  void DispatchNotifierGroupChanged();

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

  // The list of all configurable notifier groups. This is each profile that is
  // loaded (and in the ProfileAttributesStorage - so no incognito profiles go
  // here).
  std::vector<std::unique_ptr<message_center::ProfileNotifierGroup>>
      notifier_groups_;

  // Notifier source for each notifier type.
  std::map<message_center::NotifierId::NotifierType,
           std::unique_ptr<NotifierController>>
      sources_;

  size_t current_notifier_group_;

  ProfileAttributesStorage& profile_attributes_storage_;

  base::WeakPtrFactory<MessageCenterSettingsController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterSettingsController);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_
