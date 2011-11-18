// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_CHROMEOS_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/status_icons/status_tray.h"

class Profile;

class StatusTrayChromeOS : public StatusTray {
 public:
  StatusTrayChromeOS();
  virtual ~StatusTrayChromeOS();

 protected:
  // StatusTray methods:
  virtual StatusIcon* CreatePlatformStatusIcon() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(StatusTrayChromeOS);
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_CHROMEOS_H_
