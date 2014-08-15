// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/extensions/app_icon_loader.h"
#include "chrome/common/content_settings.h"
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
class ProfileInfoCache;

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
#if defined(OS_CHROMEOS)
      public user_manager::UserManager::UserSessionStateObserver,
#endif
      public extensions::AppIconLoader::Delegate {
 public:
  explicit MessageCenterSettingsController(
      ProfileInfoCache* profile_info_cache);
  virtual ~MessageCenterSettingsController();

  // Overridden from message_center::NotifierSettingsProvider.
  virtual void AddObserver(
      message_center::NotifierSettingsObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      message_center::NotifierSettingsObserver* observer) OVERRIDE;
  virtual size_t GetNotifierGroupCount() const OVERRIDE;
  virtual const message_center::NotifierGroup& GetNotifierGroupAt(
      size_t index) const OVERRIDE;
  virtual bool IsNotifierGroupActiveAt(size_t index) const OVERRIDE;
  virtual void SwitchToNotifierGroup(size_t index) OVERRIDE;
  virtual const message_center::NotifierGroup& GetActiveNotifierGroup() const
      OVERRIDE;
  virtual void GetNotifierList(
      std::vector<message_center::Notifier*>* notifiers) OVERRIDE;
  virtual void SetNotifierEnabled(const message_center::Notifier& notifier,
                                  bool enabled) OVERRIDE;
  virtual void OnNotifierSettingsClosing() OVERRIDE;
  virtual bool NotifierHasAdvancedSettings(
      const message_center::NotifierId& notifier_id) const OVERRIDE;
  virtual void OnNotifierAdvancedSettingsRequested(
      const message_center::NotifierId& notifier_id,
      const std::string* notification_id) OVERRIDE;

#if defined(OS_CHROMEOS)
  // Overridden from user_manager::UserManager::UserSessionStateObserver.
  virtual void ActiveUserChanged(
      const user_manager::User* active_user) OVERRIDE;
#endif

  // Overridden from extensions::AppIconLoader::Delegate.
  virtual void SetAppImage(const std::string& id,
                           const gfx::ImageSkia& image) OVERRIDE;

 private:
  // Overridden from content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void OnFaviconLoaded(const GURL& url,
                       const favicon_base::FaviconImageResult& favicon_result);

#if defined(OS_CHROMEOS)
  // Sets up the notifier group for the guest session. This needs to be
  // separated from RebuildNotifierGroup() and called asynchronously to avoid
  // the infinite loop of creating profile. See more the comment of
  // RebuildNotifierGroups().
  void CreateNotifierGroupForGuestLogin();
#endif

  void RebuildNotifierGroups();

  // The views displaying notifier settings.
  ObserverList<message_center::NotifierSettingsObserver> observers_;

  // The task tracker for loading favicons.
  scoped_ptr<base::CancelableTaskTracker> favicon_tracker_;

  scoped_ptr<extensions::AppIconLoader> app_icon_loader_;

  std::map<base::string16, ContentSettingsPattern> patterns_;

  // The list of all configurable notifier groups. This is each profile that is
  // loaded (and in the ProfileInfoCache - so no incognito profiles go here).
  ScopedVector<message_center::ProfileNotifierGroup> notifier_groups_;
  size_t current_notifier_group_;

  content::NotificationRegistrar registrar_;

  ProfileInfoCache* profile_info_cache_;

  base::WeakPtrFactory<MessageCenterSettingsController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterSettingsController);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_SETTINGS_CONTROLLER_H_
