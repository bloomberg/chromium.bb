// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/scoped_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/message_center/notifier_settings.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry_observer.h"
#endif

class Profile;

#if defined(ENABLE_EXTENSIONS)
namespace extensions {
class ExtensionRegistry;
}
#endif

namespace user_prefs {
class PrefRegistrySyncable;
}

// The DesktopNotificationService is an object, owned by the Profile,
// which provides the creation of desktop "toasts" to web pages and workers.
class DesktopNotificationService : public KeyedService
#if defined(ENABLE_EXTENSIONS)
                                 , public extensions::ExtensionRegistryObserver
#endif
                                   {
 public:
  // Register profile-specific prefs of notifications.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* prefs);

  explicit DesktopNotificationService(Profile* profile);
  ~DesktopNotificationService() override;

  // Returns true if the notifier with |notifier_id| is allowed to send
  // notifications.
  bool IsNotifierEnabled(const message_center::NotifierId& notifier_id) const;

  // Updates the availability of the notifier.
  void SetNotifierEnabled(const message_center::NotifierId& notifier_id,
                          bool enabled);

 private:
  // Called when the string list pref has been changed.
  void OnStringListPrefChanged(
      const char* pref_name, std::set<std::string>* ids_field);

  // Called when the disabled_extension_id pref has been changed.
  void OnDisabledExtensionIdsChanged();

  void FirePermissionLevelChangedEvent(
      const message_center::NotifierId& notifier_id,
      bool enabled);

#if defined(ENABLE_EXTENSIONS)
  // extensions::ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;
#endif

  // The profile which owns this object.
  Profile* profile_;

  // Prefs listener for disabled_extension_id.
  StringListPrefMember disabled_extension_id_pref_;

  // Prefs listener for disabled_system_component_id.
  StringListPrefMember disabled_system_component_id_pref_;

  // On-memory data for the availability of extensions.
  std::set<std::string> disabled_extension_ids_;

  // On-memory data for the availability of system_component.
  std::set<std::string> disabled_system_component_ids_;

#if defined(ENABLE_EXTENSIONS)
  // An observer to listen when extension is uninstalled.
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DesktopNotificationService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_
