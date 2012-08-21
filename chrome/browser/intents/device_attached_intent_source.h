// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_DEVICE_ATTACHED_INTENT_SOURCE_H_
#define CHROME_BROWSER_INTENTS_DEVICE_ATTACHED_INTENT_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "base/system_monitor/system_monitor.h"

class Browser;

namespace content {
class WebContentsDelegate;
}

// Listens for media devices attached to the system. When such are detected,
// translates the notification into a Web Intents dispatch.
// The intent payload is:
//   action = "chrome-extension://attach"
//   type = "chrome-extension://filesystem"
//   root_path = the File Path at which the device is accessible
//   filesystem_id = registered isolated file system identifier
class DeviceAttachedIntentSource
    : public base::SystemMonitor::DevicesChangedObserver,
      public base::SupportsWeakPtr<DeviceAttachedIntentSource> {
 public:
  DeviceAttachedIntentSource(Browser* browser,
                             content::WebContentsDelegate* delegate);
  virtual ~DeviceAttachedIntentSource();

  // base::SystemMonitor::DevicesChangedObserver implementation.
  virtual void OnMediaDeviceAttached(
      const std::string& id,
      const string16& name,
      const FilePath::StringType& location) OVERRIDE;
  virtual void OnMediaDeviceDetached(const std::string& id) OVERRIDE;

  // Dispatches web intents for the attached media device specified by
  // |device_info|.
  void DispatchIntentsForService(
      const base::SystemMonitor::MediaDeviceInfo& device_info);

 private:
  typedef std::map<std::string, base::SystemMonitor::MediaDeviceInfo>
      DeviceIdToInfoMap;

  // Weak pointer to browser to which intents will be dispatched.
  Browser* browser_;
  content::WebContentsDelegate* delegate_;
  DeviceIdToInfoMap device_id_map_;

  DISALLOW_COPY_AND_ASSIGN(DeviceAttachedIntentSource);
};

#endif  // CHROME_BROWSER_INTENTS_DEVICE_ATTACHED_INTENT_SOURCE_H_
