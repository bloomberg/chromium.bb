// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_DEVICE_ATTACHED_INTENT_SOURCE_H_
#define CHROME_BROWSER_INTENTS_DEVICE_ATTACHED_INTENT_SOURCE_H_

#include "base/system_monitor/system_monitor.h"

class FilePath;
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
    : public base::SystemMonitor::DevicesChangedObserver {
 public:
  explicit DeviceAttachedIntentSource(Browser* browser,
                                      content::WebContentsDelegate* delegate);
  virtual ~DeviceAttachedIntentSource();

  // DevicesChangedObserver
  virtual void OnMediaDeviceAttached(
      const base::SystemMonitor::DeviceIdType& id,
      const std::string& name,
      const FilePath& path) OVERRIDE;

 private:
  // Weak pointer to browser to which intents will be dispatched.
  Browser* browser_;
  content::WebContentsDelegate* delegate_;
};

#endif  // CHROME_BROWSER_INTENTS_DEVICE_ATTACHED_INTENT_SOURCE_H_
