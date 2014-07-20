// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_STORAGE_MONITOR_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_STORAGE_MONITOR_H_

#include <set>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

namespace gfx {
class Image;
}

namespace extensions {

class Extension;
class ExtensionPrefs;
class ExtensionRegistry;
class StorageEventObserver;

// ExtensionStorageMonitor monitors the storage usage of extensions and apps
// that are granted unlimited storage and displays notifications when high
// usage is detected.
class ExtensionStorageMonitor : public KeyedService,
                                public content::NotificationObserver,
                                public ExtensionRegistryObserver,
                                public ExtensionUninstallDialog::Delegate {
 public:
  static ExtensionStorageMonitor* Get(content::BrowserContext* context);

  // Indices of buttons in the notification. Exposed for testing.
  enum ButtonIndex {
    BUTTON_DISABLE_NOTIFICATION = 0,
    BUTTON_UNINSTALL
  };

  explicit ExtensionStorageMonitor(content::BrowserContext* context);
  virtual ~ExtensionStorageMonitor();

 private:
  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ExtensionRegistryObserver overrides:
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;
  virtual void OnExtensionWillBeInstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      bool is_update,
      bool from_ephemeral,
      const std::string& old_name) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;

  // Overridden from ExtensionUninstallDialog::Delegate:
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

  std::string GetNotificationId(const std::string& extension_id);

  void OnStorageThresholdExceeded(const std::string& extension_id,
                                  int64 next_threshold,
                                  int64 current_usage);
  void OnImageLoaded(const std::string& extension_id,
                     int64 current_usage,
                     const gfx::Image& image);
  void OnNotificationButtonClick(const std::string& extension_id,
                                 int button_index);

  void DisableStorageMonitoring(const std::string& extension_id);
  void StartMonitoringStorage(const Extension* extension);
  void StopMonitoringStorage(const std::string& extension_id);
  void StopMonitoringAll();

  void RemoveNotificationForExtension(const std::string& extension_id);
  void RemoveAllNotifications();

  // Displays the prompt for uninstalling the extension.
  void ShowUninstallPrompt(const std::string& extension_id);

  // Returns/sets the next threshold for displaying a notification if an
  // extension or app consumes excessive disk space.
  int64 GetNextStorageThreshold(const std::string& extension_id) const;
  void SetNextStorageThreshold(const std::string& extension_id,
                               int64 next_threshold);

  // Returns the raw next storage threshold value stored in prefs. Returns 0 if
  // the initial threshold has not yet been reached.
  int64 GetNextStorageThresholdFromPrefs(const std::string& extension_id) const;

  // Returns/sets whether notifications should be shown if an extension or app
  // consumes too much disk space.
  bool IsStorageNotificationEnabled(const std::string& extension_id) const;
  void SetStorageNotificationEnabled(const std::string& extension_id,
                                     bool enable_notifications);

  // Initially, monitoring will only be applied to ephemeral apps. This flag
  // is set by tests to enable for all extensions and apps.
  bool enable_for_all_extensions_;

  // The first notification is shown after the initial threshold is exceeded.
  // Ephemeral apps have a lower threshold than fully installed extensions.
  // A lower threshold is set by tests.
  int64 initial_extension_threshold_;
  int64 initial_ephemeral_threshold_;

  // The rate (in seconds) at which we would like to receive storage updates
  // from QuotaManager. Overridden in tests.
  int observer_rate_;

  // IDs of extensions that notifications were shown for.
  std::set<std::string> notified_extension_ids_;

  content::BrowserContext* context_;
  extensions::ExtensionPrefs* extension_prefs_;

  content::NotificationRegistrar registrar_;
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  // StorageEventObserver monitors storage for extensions on the IO thread.
  scoped_refptr<StorageEventObserver> storage_observer_;

  // Modal dialog used to confirm removal of an extension.
  scoped_ptr<ExtensionUninstallDialog> uninstall_dialog_;

  // The ID of the extension that is the subject of the uninstall confirmation
  // dialog.
  std::string uninstall_extension_id_;

  base::WeakPtrFactory<ExtensionStorageMonitor> weak_ptr_factory_;

  friend class StorageEventObserver;
  friend class ExtensionStorageMonitorTest;

  DISALLOW_COPY_AND_ASSIGN(ExtensionStorageMonitor);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_STORAGE_MONITOR_H_
