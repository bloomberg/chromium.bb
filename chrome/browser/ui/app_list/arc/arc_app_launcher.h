// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_LAUNCHER_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_LAUNCHER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

namespace content {
class BrowserContext;
}

// Helper class for deferred ARC app launching in case app is not ready on the
// moment of request.
class ArcAppLauncher : public ArcAppListPrefs::Observer {
 public:
  ArcAppLauncher(content::BrowserContext* context,
                 const std::string& app_id,
                 bool landscape_layout);
  ~ArcAppLauncher() override;

  bool app_launched() const { return app_launched_; }

  // ArcAppListPrefs::Observer:
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppReadyChanged(const std::string& app_id, bool ready) override;

 private:
  void LaunchApp();

  // Unowned pointer.
  content::BrowserContext* context_;
  // ARC app id and requested layout.
  const std::string app_id_;
  const bool landscape_layout_;
  // Flag idicating that ARC app was launched.
  bool app_launched_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcAppLauncher);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_LAUNCHER_H_
