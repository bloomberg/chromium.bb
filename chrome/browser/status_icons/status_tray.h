// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_H_
#define CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_H_

#include "base/hash_tables.h"
#include "base/scoped_ptr.h"

class StatusIcon;

// Factory interface for creating icons to improve testability.
class StatusIconFactory {
 public:
  virtual StatusIcon* CreateIcon() = 0;
  virtual ~StatusIconFactory() { }
};

// Provides a cross-platform interface to the system's status tray, and exposes
// APIs to add/remove icons to the tray and attach context menus.
class StatusTray {
 public:
  // Takes ownership of |factory|.
  explicit StatusTray(StatusIconFactory* factory);
  ~StatusTray();

  // Gets the current status icon associated with this identifier, or creates
  // a new one if none exists. The StatusTray retains ownership of the
  // StatusIcon. Returns NULL if the status tray icon could not be created.
  StatusIcon* GetStatusIcon(const string16& identifier);

  // Removes the current status icon associated with this identifier, if any.
  void RemoveStatusIcon(const string16& identifier);

 private:
  typedef base::hash_map<string16, StatusIcon*> StatusIconMap;
  // Map containing all active StatusIcons.
  // Key: String identifiers (passed in to GetStatusIcon)
  // Value: The StatusIcon associated with that identifier (strong pointer -
  //   StatusIcons are freed when the StatusTray destructor is called).
  StatusIconMap status_icons_;

  scoped_ptr<StatusIconFactory> factory_;
};

#endif  // CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_H_
