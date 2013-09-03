// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_VOLUME_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_VOLUME_MANAGER_OBSERVER_H_

#include <string>

namespace file_manager {

struct VolumeInfo;

// Observer interface of volume related events.
class VolumeManagerObserver {
 public:
  virtual ~VolumeManagerObserver() {}

  // Fired when a new device is added.
  virtual void OnDeviceAdded(const std::string& device_path) = 0;

  // Fired when a new device is removed.
  virtual void OnDeviceRemoved(const std::string& device_path) = 0;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_VOLUME_MANAGER_OBSERVER_H_
