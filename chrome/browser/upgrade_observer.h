// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_OBSERVER_H_
#define CHROME_BROWSER_UPGRADE_OBSERVER_H_

#include "base/macros.h"

class UpgradeObserver {
 public:
  // Triggered when a software update is available, but downloading requires
  // user's agreement as current connection is cellular.
  virtual void OnUpdateOverCellularAvailable() {}

  // Triggered when Chrome believes an update has been installed and available
  // for long enough with the user shutting down to let it take effect. See
  // upgrade_detector.cc for details on how long it waits. No details are
  // expected.
  virtual void OnUpgradeRecommended() {}

  // Triggered when a critical update has been installed. No details are
  // expected.
  virtual void OnCriticalUpgradeInstalled() {}

  // Triggered when the current install is outdated. No details are expected.
  virtual void OnOutdatedInstall() {}

  // Triggered when the current install is outdated and auto-update (AU) is
  // disabled. No details are expected.
  virtual void OnOutdatedInstallNoAutoUpdate() {}

 protected:
  virtual ~UpgradeObserver() {}
};

#endif  // CHROME_BROWSER_UPGRADE_OBSERVER_H_
