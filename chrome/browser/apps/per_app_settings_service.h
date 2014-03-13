// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PER_APP_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_APPS_PER_APP_SETTINGS_SERVICE_H_

#include <map>
#include <string>

#include "chrome/browser/ui/host_desktop.h"
#include "components/keyed_service/core/keyed_service.h"

// Stores settings for apps that only persist until the browser context is
// destroyed.
class PerAppSettingsService : public KeyedService {
 public:
  PerAppSettingsService();
  virtual ~PerAppSettingsService();

  // Sets/gets the desktop that |app_id| was last launched from.
  void SetDesktopLastLaunchedFrom(
      const std::string& app_id, chrome::HostDesktopType host);
  chrome::HostDesktopType GetDesktopLastLaunchedFrom(
      const std::string& app_id) const;
  bool HasDesktopLastLaunchedFrom(const std::string& app_id) const;

 private:
  typedef std::map<std::string, chrome::HostDesktopType> DesktopMap;
  DesktopMap default_desktops_;

  DISALLOW_COPY_AND_ASSIGN(PerAppSettingsService);
};

#endif  // CHROME_BROWSER_APPS_PER_APP_SETTINGS_SERVICE_H_

