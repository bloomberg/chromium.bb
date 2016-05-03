// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_CONTROLLER_HELPER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_CONTROLLER_HELPER_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"

class Profile;

namespace content {
class WebContents;
}

// Assists the LauncherController with ExtensionService interaction.
class LauncherControllerHelper {
 public:
  explicit LauncherControllerHelper(Profile* profile);
  virtual ~LauncherControllerHelper();

  // Helper function to return the title associated with |app_id|.
  // Returns an empty title if no matching extension can be found.
  static base::string16 GetAppTitle(Profile* profile,
                                    const std::string& app_id);

  // Returns the app id of the specified tab, or an empty string if there is
  // no app. All known profiles will be queried for this.
  virtual std::string GetAppID(content::WebContents* tab);

  // Returns true if |id| is valid for the currently active profile.
  // Used during restore to ignore no longer valid extensions.
  // Note that already running applications are ignored by the restore process.
  virtual bool IsValidIDForCurrentUser(const std::string& id);

  // Sets the currently active profile for the usage of |GetAppID|.
  virtual void SetCurrentUser(Profile* profile);

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(LauncherControllerHelper);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_CONTROLLER_HELPER_H_
