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
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"

namespace chromeos {

// Tracks global demo session state. For example, whether the demo session has
// started, and whether the demo session offline resources have been loaded.
class DemoSession {
 public:
  enum class EnrollmentType {
    // Demo mode enrollment unset/unknown.
    kNone,

    // Device was not enrolled into demo mode.
    kUnenrolled,

    // Device was enrolled into demo mode using online enrollment flow.
    kOnline,

    // Device was enrolled into demo mode using offline enrollment flow - i.e.
    // policies are retrieved from the device, rather than from DM server.
    kOffline,
  };

  // Location on disk where pre-installed demo mode resources are expected to be
  // found.
  static base::FilePath GetPreInstalledDemoResourcesPath();
  static void OverridePreInstalledDemoResourcesPathForTesting(
      base::FilePath* overriden_path);

  // Whether the device is set up to run demo sessions.
  static bool IsDeviceInDemoMode();

  // Returns the type of demo mode setup.
  static EnrollmentType GetEnrollmentType();

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

  // Gets the path under offline demo resources mount point that contains
  // external extensions prefs (JSON containing set of extensions to be loaded
  // as external extensions into demo sessions - expected to map extension IDs
  // to the associated CRX path and version).
  base::FilePath GetExternalExtensionsPrefsPath() const;

  // Converts a relative path to an absolute path under the offline demo
  // resources mount. Returns an empty string if the offline demo resources are
  // not loaded.
  base::FilePath GetOfflineResourceAbsolutePath(
      const base::FilePath& relative_path) const;

  bool offline_enrolled() const { return offline_enrolled_; }

  bool started() const { return started_; }

  bool offline_resources_loaded() const { return offline_resources_loaded_; }

 private:
  DemoSession();
  ~DemoSession();

  // Called after load of a currently installed (if any) demo mode resources
  // component has finished.
  // On success, |path| is expected to contain the path as which the component
  // is loaded.
  void InstalledComponentLoaded(
      component_updater::CrOSComponentManager::Error error,
      const base::FilePath& path);

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
