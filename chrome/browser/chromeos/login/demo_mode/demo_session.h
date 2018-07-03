// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SESSION_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SESSION_H_

#include <list>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"

namespace chromeos {

// Tracks global demo session state. For example, whether the demo session has
// started, and whether the demo session offline resources have been loaded.
class DemoSession {
 public:
  enum class EnrollmentType {
    // Device was not enrolled into demo mode
    kNone,

    // Device was enrolled into demo mode using online enrollment flow.
    kOnline,

    // Device was enrolled into demo mode using offline enrollment flow - i.e.
    // policies are retrieved from the device, rather than from DM server.
    kOffline,
  };

  // Whether the device is set up to run demo sessions.
  static bool IsDeviceInDemoMode();

  static void SetDemoModeEnrollmentTypeForTesting(
      EnrollmentType enrollment_type);

  // If the device is set up to run in demo mode, marks demo session as started,
  // and requests load of demo session resources.
  // Creates global DemoSession instance if required.
  static DemoSession* StartIfInDemoMode();

  // Requests load of demo session resources, without marking the demo session
  // as started. Creates global DemoSession instance if required.
  static void PreloadOfflineResourcesIfInDemoMode();

  // Deletes the global DemoSession instance if it was previously created.
  static void ShutDownIfInitialized();

  // Gets the global demo session instance. Returns nullptr if the DemoSession
  // instance has not yet been initialized (either by calling
  // StartIfInDemoMode() or PreloadOfflineResourcesIfInDemoMode()).
  static DemoSession* Get();

  // Ensures that the load of offline demo session resources is requested.
  // |load_callback| will be run once the offline resource load finishes.
  void EnsureOfflineResourcesLoaded(base::OnceClosure load_callback);

  // Gets the path of the image containing demo session Android apps. The path
  // will be set when the offline resources get loaded.
  base::FilePath GetDemoAppsPath() const;

  bool offline_enrolled() const { return offline_enrolled_; }

  bool started() const { return started_; }

  bool offline_resources_loaded() const { return offline_resources_loaded_; }

 private:
  DemoSession();
  ~DemoSession();

  // Callback for the image loader request to load offline demo mode resources.
  // |mount_path| is the path at which the resources were loaded.
  void OnOfflineResourcesLoaded(base::Optional<base::FilePath> mounted_path);

  // Whether the device was offline-enrolled into demo mode, i.e. enrolled using
  // pre-built policies. Offline enrolled demo sessions do not have working
  // robot account associated with them.
  bool offline_enrolled_ = false;

  // Whether demo session has been started.
  bool started_ = false;

  bool offline_resources_load_requested_ = false;
  bool offline_resources_loaded_ = false;

  // Path at which offline demo mode resources were loaded.
  base::FilePath offline_resources_path_;

  // List of pending callbacks passed to EnsureOfflineResourcesLoaded().
  std::list<base::OnceClosure> offline_resources_load_callbacks_;

  base::WeakPtrFactory<DemoSession> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DemoSession);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SESSION_H_
