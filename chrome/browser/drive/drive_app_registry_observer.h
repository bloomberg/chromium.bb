// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DRIVE_DRIVE_APP_REGISTRY_OBSERVER_H_
#define CHROME_BROWSER_DRIVE_DRIVE_APP_REGISTRY_OBSERVER_H_

namespace drive {

class DriveAppRegistryObserver {
 public:
  // Invoked when DriveAppRegistry has updated its list of Drive apps.
  virtual void OnDriveAppRegistryUpdated() = 0;

 protected:
  virtual ~DriveAppRegistryObserver() {}
};

}  // namespace drive

#endif  // CHROME_BROWSER_DRIVE_DRIVE_APP_REGISTRY_OBSERVER_H_
