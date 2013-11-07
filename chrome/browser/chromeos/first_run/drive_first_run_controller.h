// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CHROMEOS_FIRST_RUN_DRIVE_FIRST_RUN_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_FIRST_RUN_DRIVE_FIRST_RUN_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {

class DriveWebContentsManager;

// This class is responsible for kicking off the Google Drive offline
// initialization process. There is an initial delay to avoid contention when
// the session starts. DriveFirstRunController will manage its own lifetime and
// destroy itself when the initialization succeeds or fails.
class DriveFirstRunController {
 public:
  DriveFirstRunController();
  ~DriveFirstRunController();

  // Starts the process to enable offline mode for the user's Drive account.
  void EnableOfflineMode();

 private:
  // Used as a callback to indicate whether the offline initialization
  // succeeds or fails.
  void OnOfflineInit(bool success);

  // Called when timed out waiting for offline initialization to complete.
  void OnWebContentsTimedOut();

  // Cleans up internal state and schedules self for deletion.
  void CleanUp();

  Profile* profile_;
  scoped_ptr<DriveWebContentsManager> web_contents_manager_;
  base::OneShotTimer<DriveFirstRunController> web_contents_timer_;
  base::OneShotTimer<DriveFirstRunController> initial_delay_timer_;
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(DriveFirstRunController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FIRST_RUN_DRIVE_FIRST_RUN_CONTROLLER_H_
