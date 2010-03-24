// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_H_
#define CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_H_

#include "base/hash_tables.h"
#include "base/scoped_ptr.h"

class StatusIcon;

// Provides a cross-platform interface to the system's status tray, and exposes
// APIs to add/remove icons to the tray and attach context menus.
class StatusTray {
 public:
  // Static factory method that is implemented separately for each platform to
  // produce the appropriate platform-specific instance.
  static StatusTray* Create();

  virtual ~StatusTray();

  // Gets the current status icon associated with this identifier, or creates
  // a new one if none exists. The StatusTray retains ownership of the
  // StatusIcon. Returns NULL if the status tray icon could not be created.
  StatusIcon* GetStatusIcon(const string16& identifier);

  // Removes the current status icon associated with this identifier, if any.
  void RemoveStatusIcon(const string16& identifier);

 protected:
  StatusTray();
  // Factory method for creating a status icon.
  virtual StatusIcon* CreateStatusIcon() = 0;

  // Removes all StatusIcons (used by derived classes to clean up in case they
  // track external state used by the StatusIcons).
  void RemoveAllIcons();

  typedef base::hash_map<string16, StatusIcon*> StatusIconMap;
  // Returns the list of active status icons so subclasses can operate on them.
  const StatusIconMap& status_icons() { return status_icons_; }

 private:
  // Map containing all active StatusIcons.
  // Key: String identifiers (passed in to GetStatusIcon)
  // Value: The StatusIcon associated with that identifier (strong pointer -
  //   StatusIcons are freed when the StatusTray destructor is called).
  StatusIconMap status_icons_;

  DISALLOW_COPY_AND_ASSIGN(StatusTray);
};

#endif  // CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_H_
