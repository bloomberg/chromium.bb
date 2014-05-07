// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_RUNTIME_CHROME_RUNTIME_API_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_RUNTIME_CHROME_RUNTIME_API_DELEGATE_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/api/runtime/runtime_api_delegate.h"

namespace base {
class TimeTicks;
}

namespace content {
class BrowserContext;
class NotificationDetails;
class NotificationSource;
}

namespace extensions {
class RuntimeAPI;
class UpdateObserver;
}

class ChromeRuntimeAPIDelegate : public extensions::RuntimeAPIDelegate,
                                 public content::NotificationObserver {
 public:
  explicit ChromeRuntimeAPIDelegate(content::BrowserContext* context);
  virtual ~ChromeRuntimeAPIDelegate();

 private:
  friend class extensions::RuntimeAPI;

  // extensions::RuntimeAPIDelegate implementation.
  virtual void AddUpdateObserver(extensions::UpdateObserver* observer) OVERRIDE;
  virtual void RemoveUpdateObserver(
      extensions::UpdateObserver* observer) OVERRIDE;
  virtual base::Version GetPreviousExtensionVersion(
      const extensions::Extension* extension) OVERRIDE;
  virtual void ReloadExtension(const std::string& extension_id) OVERRIDE;
  virtual bool CheckForUpdates(const std::string& extension_id,
                               const UpdateCheckCallback& callback) OVERRIDE;
  virtual void OpenURL(const GURL& uninstall_url) OVERRIDE;
  virtual bool GetPlatformInfo(
      extensions::core_api::runtime::PlatformInfo* info) OVERRIDE;
  virtual bool RestartDevice(std::string* error_message) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void UpdateCheckComplete(const std::string& extension_id);
  void CallUpdateCallbacks(const std::string& extension_id,
                           const UpdateCheckResult& result);

  content::BrowserContext* browser_context_;

  content::NotificationRegistrar registrar_;

  // Whether the API registered with the ExtensionService to receive
  // update notifications.
  bool registered_for_updates_;

  // Map to prevent extensions from getting stuck in reload loops. Maps
  // extension id to the last time it was reloaded and the number of times
  // it was reloaded with not enough time in between reloads.
  std::map<std::string, std::pair<base::TimeTicks, int> > last_reload_time_;

  // Pending update checks.
  typedef std::vector<UpdateCheckCallback> UpdateCallbackList;
  typedef std::map<std::string, UpdateCallbackList> UpdateCallbackMap;
  UpdateCallbackMap pending_update_checks_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeRuntimeAPIDelegate);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_RUNTIME_CHROME_RUNTIME_API_DELEGATE_H_
