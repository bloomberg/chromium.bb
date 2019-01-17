// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHARE_PATH_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHARE_PATH_H_

#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_observer.h"
#include "chromeos/dbus/seneschal/seneschal_service.pb.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace crostini {

// Handles sharing and unsharing paths from the Chrome OS host to the crostini
// VM via seneschal.
class CrostiniSharePath : public KeyedService,
                          public file_manager::VolumeManagerObserver {
 public:
  using SharePathCallback =
      base::OnceCallback<void(const base::FilePath&, bool, std::string)>;
  using MountEventSeneschalCallback =
      base::RepeatingCallback<void(const std::string& operation,
                                   const base::FilePath& cros_path,
                                   const base::FilePath& container_path,
                                   bool result,
                                   std::string failure_reason)>;
  class Observer {
   public:
    virtual void OnUnshare(const base::FilePath& path) = 0;
  };

  static CrostiniSharePath* GetForProfile(Profile* profile);
  explicit CrostiniSharePath(Profile* profile);
  ~CrostiniSharePath() override;

  // Observer receives unshare events.
  void AddObserver(Observer* obs);

  // Share specified absolute |path| with vm. If |persist| is set, the path will
  // be automatically shared at container startup. Callback receives path mapped
  // in container, success bool and failure reason string.
  void SharePath(std::string vm_name,
                 const base::FilePath& path,
                 bool persist,
                 SharePathCallback callback);

  // Share specified absolute |paths| with vm. If |persist| is set, the paths
  // will be automatically shared at container startup. Callback receives
  // success bool and failure reason string of the first error.
  void SharePaths(std::string vm_name,
                  std::vector<base::FilePath> paths,
                  bool persist,
                  base::OnceCallback<void(bool, std::string)> callback);

  // Unshare specified |path| with vm.
  // Callback receives success bool and failure reason string.
  void UnsharePath(std::string vm_name,
                   const base::FilePath& path,
                   base::OnceCallback<void(bool, std::string)> callback);

  // Returns true the first time it is called on this service.
  bool GetAndSetFirstForSession();

  // Get list of all shared paths for the default crostini container.
  std::vector<base::FilePath> GetPersistedSharedPaths();

  // Share all paths configured in prefs for the default crostini container.
  // Called at container startup.  Callback is invoked once complete.
  void SharePersistedPaths(
      base::OnceCallback<void(bool, std::string)> callback);

  // Save |path| into prefs.
  void RegisterPersistedPath(const base::FilePath& path);

  // file_manager::VolumeManagerObserver
  void OnVolumeMounted(chromeos::MountError error_code,
                       const file_manager::Volume& volume) override;
  void OnVolumeUnmounted(chromeos::MountError error_code,
                         const file_manager::Volume& volume) override;

  // Allow seneschal callback for mount events to be overridden for testing.
  void SetMountEventSeneschalCallbackForTesting(
      MountEventSeneschalCallback callback) {
    mount_event_seneschal_callback_ = std::move(callback);
  }

 private:
  void CallSeneschalSharePath(std::string vm_name,
                              const base::FilePath& path,
                              bool persist,
                              SharePathCallback callback);

  void CallSeneschalUnsharePath(
      std::string vm_name,
      const base::FilePath& path,
      base::OnceCallback<void(bool, std::string)> callback);

  Profile* profile_;
  bool first_for_session_ = true;

  // Allow callback for mount event to be overidden for testing.
  MountEventSeneschalCallback mount_event_seneschal_callback_;
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniSharePath);
};  // class

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHARE_PATH_H_
