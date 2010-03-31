// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_MANAGER_H_
#define CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_MANAGER_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/status_icons/status_icon.h"

class Profile;
class StatusTray;

// Manages the set of status tray icons and associated UI.
class StatusTrayManager : private StatusIcon::Observer {
 public:
  StatusTrayManager();
  virtual ~StatusTrayManager();

  void Init(Profile* profile);

 private:
  // Overriden from StatusIcon::Observer:
  virtual void OnClicked();

  scoped_ptr<StatusTray> status_tray_;
  Profile* profile_;
};

#endif  // CHROME_BROWSER_STATUS_ICONS_STATUS_TRAY_MANAGER_H_
