// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_INFO_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_INFO_EVENT_ROUTER_H_

#include <set>

#include "base/file_path.h"
#include "base/memory/singleton.h"
#include "base/values.h"

namespace extensions {

// Event router for systemInfo API. It is a singleton instance shared by
// multiple profiles.
// TODO(hongbo): It should derive from SystemMonitor::DevicesChangedObserver.
// Since the system_monitor will be refactored along with media_gallery, once
// http://crbug.com/145400 is fixed, we need to update SystemInfoEventRouter
// accordingly.
class SystemInfoEventRouter {
 public:
  static SystemInfoEventRouter* GetInstance();

  // Add/remove event listener for the |event_name| event from |profile|.
  void AddEventListener(const std::string& event_name);
  void RemoveEventListener(const std::string& event_name);

  // Return true if the |event_name| is an event from systemInfo namespace.
  static bool IsSystemInfoEvent(const std::string& event_name);

  // TODO(hongbo): The following methods should be likely overriden from
  // SystemMonitor::DevicesChangedObserver once the http://crbug.com/145400
  // is fixed.
  void OnStorageAvailableCapacityChanged(const std::string& id,
                                         int64 available_capacity);
  void OnRemovableStorageAttached(const std::string& id,
                                  const string16& name,
                                  const FilePath::StringType& location);
  void OnRemovableStorageDetached(const std::string& id);

 private:
  friend struct DefaultSingletonTraits<SystemInfoEventRouter>;

  SystemInfoEventRouter();
  virtual ~SystemInfoEventRouter();

  // Called on the UI thread to dispatch the systemInfo event to all extension
  // processes cross multiple profiles.
  void DispatchEvent(const std::string& event_name,
      scoped_ptr<base::ListValue> args);

  // Used to record the event names being watched.
  std::multiset<std::string> watching_event_set_;

  DISALLOW_COPY_AND_ASSIGN(SystemInfoEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_INFO_EVENT_ROUTER_H_
