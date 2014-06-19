// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_DRIVE_DRIVE_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_APPS_DRIVE_DRIVE_SERVICE_BRIDGE_H_

#include <set>

#include "base/memory/scoped_ptr.h"

namespace drive {
class DriveAppRegistry;
}

class BrowserContextKeyedServiceFactory;
class Profile;

// An interface to access Drive service for a given profile.
class DriveServiceBridge {
 public:
  virtual ~DriveServiceBridge() {}

  // Factory to create an instance of DriveServiceBridge.
  static scoped_ptr<DriveServiceBridge> Create(Profile* profile);

  // Appends PKS factories this class depends on.
  static void AppendDependsOnFactories(
      std::set<BrowserContextKeyedServiceFactory*>* factories);

  // Returns the DriveAppRegistery to use. The ownership is not transferred.
  virtual drive::DriveAppRegistry* GetAppRegistry() = 0;
};

#endif  // CHROME_BROWSER_APPS_DRIVE_DRIVE_SERVICE_BRIDGE_H_
