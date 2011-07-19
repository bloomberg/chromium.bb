// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_H_
#define CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"

class StatusIcon;

// Provides a cross-platform interface to the system's status tray, and exposes
// APIs to add/remove icons to the tray and attach context menus.
class StatusTray {
 public:
  // Static factory method that is implemented separately for each platform to
  // produce the appropriate platform-specific instance. Returns NULL if this
  // platform does not support status icons.
  static StatusTray* Create();

  virtual ~StatusTray();

  // Creates a new StatusIcon. The StatusTray retains ownership of the
  // StatusIcon. Returns NULL if the StatusIcon could not be created.
  StatusIcon* CreateStatusIcon();

  // Removes the current status icon associated with this identifier, if any.
  void RemoveStatusIcon(StatusIcon* icon);

 protected:
  StatusTray();
  // Factory method for creating a status icon for this platform.
  virtual StatusIcon* CreatePlatformStatusIcon() = 0;

  // Removes all StatusIcons (used by derived classes to clean up in case they
  // track external state used by the StatusIcons).
  void RemoveAllIcons();

  typedef std::vector<StatusIcon*> StatusIconList;
  // Returns the list of active status icons so subclasses can operate on them.
  const StatusIconList& status_icons() { return status_icons_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(StatusTrayTest, CreateRemove);

  // List containing all active StatusIcons.
  StatusIconList status_icons_;

  DISALLOW_COPY_AND_ASSIGN(StatusTray);
};

#endif  // CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_H_
