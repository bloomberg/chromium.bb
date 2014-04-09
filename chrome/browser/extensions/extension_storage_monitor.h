// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_STORAGE_MONITOR_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_STORAGE_MONITOR_H_

#include <set>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
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
class StorageEventObserver;

// ExtensionStorageMonitor monitors the storage usage of extensions and apps
// that are granted unlimited storage and displays notifications when high
// usage is detected.
class ExtensionStorageMonitor : public KeyedService,
                                public content::NotificationObserver,
                                public ExtensionRegistryObserver {
 public:
  static ExtensionStorageMonitor* Get(content::BrowserContext* context);

  // Indices of buttons in the notification. Exposed for testing.
  enum ButtonIndex {
    BUTTON_DISABLE_NOTIFICATION = 0
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
  virtual void OnExtensionUnloaded(content::BrowserContext* browser_context,
                                   const Extension* extension) OVERRIDE;

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

  // Initially, monitoring will only be applied to ephemeral apps. This flag
  // is set by tests to enable for all extensions and apps.
  bool enable_for_all_extensions_;

  // A lower threshold is set by tests.
  int64 initial_extension_threshold_;
  int64 initial_ephemeral_threshold_;
  int observer_rate_;

  std::set<std::string> notified_extension_ids_;

  content::BrowserContext* context_;
  content::NotificationRegistrar registrar_;
  scoped_refptr<StorageEventObserver> storage_observer_;
  base::WeakPtrFactory<ExtensionStorageMonitor> weak_ptr_factory_;

  friend class StorageEventObserver;
  friend class ExtensionStorageMonitorTest;

  DISALLOW_COPY_AND_ASSIGN(ExtensionStorageMonitor);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_STORAGE_MONITOR_H_
