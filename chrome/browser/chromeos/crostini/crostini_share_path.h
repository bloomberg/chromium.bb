// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHARE_PATH_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHARE_PATH_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_observer.h"
#include "chromeos/components/drivefs/drivefs_host_observer.h"
#include "chromeos/dbus/seneschal/seneschal_service.pb.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace crostini {

struct SharedPathInfo {
  explicit SharedPathInfo(const std::string& vm_name);
  SharedPathInfo(SharedPathInfo&&);
  ~SharedPathInfo();

  std::unique_ptr<base::FilePathWatcher> watcher;
  std::set<std::string> vm_names;
};

// Handles sharing and unsharing paths from the Chrome OS host to the crostini
// VM via seneschal.
class CrostiniSharePath : public KeyedService,
                          public file_manager::VolumeManagerObserver,
                          public drivefs::DriveFsHostObserver {
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
    virtual void OnUnshare(const std::string& vm_name,
                           const base::FilePath& path) = 0;
  };

  // Migrates from crostini.shared_paths to crostini.paths_shared_to_vms which
  // supports multi VM sharing.
  // TODO(crbug.com/946273): Remove crostini.shared_paths and migration code
  // after M77.
  static void MigratePersistedPathsToMultiVM(PrefService* profile_prefs);

  static CrostiniSharePath* GetForProfile(Profile* profile);
  explicit CrostiniSharePath(Profile* profile);
  ~CrostiniSharePath() override;

  // Observer receives unshare events.
  void AddObserver(Observer* obs);

  // Share specified absolute |path| with vm. If |persist| is set, the path will
  // be automatically shared at container startup. Callback receives path mapped
  // in container, success bool and failure reason string.
  void SharePath(const std::string& vm_name,
                 const base::FilePath& path,
                 bool persist,
                 SharePathCallback callback);

  // Share specified absolute |paths| with vm. If |persist| is set, the paths
  // will be automatically shared at container startup. Callback receives
  // success bool and failure reason string of the first error.
  void SharePaths(const std::string& vm_name,
                  std::vector<base::FilePath> paths,
                  bool persist,
                  base::OnceCallback<void(bool, std::string)> callback);

  // Unshare specified |path| with |vm_name|.  If |unpersist| is set, the path
  // is removed from prefs, and will not be shared at container startup.
  // Callback receives success bool and failure reason string.
  void UnsharePath(const std::string& vm_name,
                   const base::FilePath& path,
                   bool unpersist,
                   base::OnceCallback<void(bool, std::string)> callback);

  // Returns true the first time it is called on this service.
  bool GetAndSetFirstForSession();

  // Get list of all shared paths for the specified VM.
  std::vector<base::FilePath> GetPersistedSharedPaths(
      const std::string& vm_name);

  // Share all paths configured in prefs for the specified VM.
  // Called at container startup.  Callback is invoked once complete.
  void SharePersistedPaths(
      const std::string& vm_name,
      base::OnceCallback<void(bool, std::string)> callback);

  // Save |path| into prefs for |vm_name|.
  void RegisterPersistedPath(const std::string& vm_name,
                             const base::FilePath& path);

  // file_manager::VolumeManagerObserver
  void OnVolumeMounted(chromeos::MountError error_code,
                       const file_manager::Volume& volume) override;
  void OnVolumeUnmounted(chromeos::MountError error_code,
                         const file_manager::Volume& volume) override;

  // drivefs::DriveFsHostObserver
  void OnFilesChanged(
      const std::vector<drivefs::mojom::FileChange>& changes) override;

  // Registers |path| as shared with |vm_name|.  Adds a FilePathWatcher to
  // detect when the path has been deleted.  If the path is deleted, we unshare
  // the path, and remove it from prefs if it was persisted.
  // Visible for testing.
  void RegisterSharedPath(const std::string& vm_name,
                          const base::FilePath& path);

  // Runs on UI Thread to handle when a path is deleted.
  // Visible for testing.
  void PathDeleted(const base::FilePath& path);

  // Don't run file watchers for tests.
  void set_no_file_watchers_for_testing() {
    no_file_watchers_for_testing_ = true;
  }
  // Allow seneschal callback for mount events to be overridden for testing.
  void set_mount_event_seneschal_callback_for_testing(
      MountEventSeneschalCallback callback) {
    mount_event_seneschal_callback_ = std::move(callback);
  }

 private:
  void CallSeneschalSharePath(const std::string& vm_name,
                              const base::FilePath& path,
                              bool persist,
                              SharePathCallback callback);

  void CallSeneschalUnsharePath(
      const std::string& vm_name,
      const base::FilePath& path,
      base::OnceCallback<void(bool, std::string)> callback);

  void StartFileWatcher(const base::FilePath& path);

  // Callback for FilePathWatcher.
  void OnFileChanged(const base::FilePath& path, bool error);

  // Runs on IO Thread to check if a path is deleted.
  void CheckIfPathDeletedOnIOThread(const base::FilePath& path);

  // Returns info for specified path or nullptr if not found.
  SharedPathInfo* FindSharedPathInfo(const base::FilePath& path);

  Profile* profile_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  bool first_for_session_ = true;

  // Allow callback for mount event to be overidden for testing.
  MountEventSeneschalCallback mount_event_seneschal_callback_;
  base::ObserverList<Observer>::Unchecked observers_;
  std::map<base::FilePath, SharedPathInfo> shared_paths_;
  bool no_file_watchers_for_testing_ = false;

  DISALLOW_COPY_AND_ASSIGN(CrostiniSharePath);
};  // class

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHARE_PATH_H_
