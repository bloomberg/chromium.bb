// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_SERVICE_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_SERVICE_LAUNCHER_H_

#include <memory>

#include "base/macros.h"

class Profile;

namespace arc {

class ArcPlayStoreEnabledPreferenceHandler;
class ArcServiceManager;
class ArcSessionManager;

// Detects ARC availability and launches ARC bridge service.
class ArcServiceLauncher {
 public:
  ArcServiceLauncher();
  ~ArcServiceLauncher();

  // This is to access OnPrimaryUserProfilePrepared() only.
  static ArcServiceLauncher* Get();

  // Called before the main MessageLooop starts.
  void Initialize();

  // Called after the main MessageLoop stops, but before the Profile is
  // destroyed.
  void Shutdown();

  // Called when the main profile is initialized after user logs in.
  void OnPrimaryUserProfilePrepared(Profile* profile);

 private:
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ArcPlayStoreEnabledPreferenceHandler>
      arc_play_store_enabled_preference_handler_;

  DISALLOW_COPY_AND_ASSIGN(ArcServiceLauncher);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_SERVICE_LAUNCHER_H_
