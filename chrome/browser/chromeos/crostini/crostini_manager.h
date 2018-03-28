// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_MANAGER_H_

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/dbus/concierge/service.pb.h"

namespace crostini {

// Result types for CrostiniManager::StartTerminaVmCallback etc.
enum class ConciergeClientResult {
  SUCCESS,
  DBUS_ERROR,
  UNPARSEABLE_RESPONSE,
  CREATE_DISK_IMAGE_FAILED,
  VM_START_FAILED,
  VM_STOP_FAILED,
  CLIENT_ERROR,
  DISK_TYPE_ERROR,
  CONTAINER_START_FAILED,
  UNKNOWN_ERROR,
};

// CrostiniManager is a singleton which is used to check arguments for the
// ConciergeClient. ConciergeClient is dedicated to communication with the
// Concierge service and should remain as thin as possible.
class CrostiniManager {
 public:
  // The type of the callback for CrostiniManager::StartVmConcierge.
  using StartVmConciergeCallback =
      base::OnceCallback<void(bool is_service_available)>;
  // The type of the callback for CrostiniManager::StartTerminaVm.
  using StartTerminaVmCallback =
      base::OnceCallback<void(ConciergeClientResult result)>;
  // The type of the callback for CrostiniManager::CreateDiskImage.
  using CreateDiskImageCallback =
      base::OnceCallback<void(ConciergeClientResult result,
                              const base::FilePath& disk_path)>;
  // The type of the callback for CrostiniManager::StopVm.
  using StopVmCallback = base::OnceCallback<void(ConciergeClientResult result)>;
  // The type of the callback for CrostiniManager::StartContainer.
  using StartContainerCallback =
      base::OnceCallback<void(ConciergeClientResult result)>;

  // Starts the Concierge service. |callback| is called after the method call
  // finishes.
  void StartVmConcierge(StartVmConciergeCallback callback);

  // Checks the arguments for creating a new Termina VM disk image. Creates a
  // disk image for a Termina VM via ConciergeClient::CreateDiskImage.
  // |callback| is called if the arguments are bad, or after the method call
  // finishes.
  void CreateDiskImage(
      // The cryptohome id for the user's encrypted storage.
      const std::string& cryptohome_id,
      // The path to the disk image, including the name of
      // the image itself. The image name should match the
      // name of the VM that it will be used for.
      const base::FilePath& disk_path,
      // The storage location for the disk image
      vm_tools::concierge::StorageLocation storage_location,
      CreateDiskImageCallback callback);

  // Checks the arguments for starting a Termina VM. Starts a Termina VM via
  // ConciergeClient::StartTerminaVm. |callback| is called if the arguments
  // are bad, or after the method call finishes.
  void StartTerminaVm(
      // The human-readable name to be assigned to this VM.
      std::string name,
      // Path to the disk image on the host.
      const base::FilePath& disk_path,
      StartTerminaVmCallback callback);

  // Checks the arguments for stopping a Termina VM. Stops the Termina VM via
  // ConciergeClient::StopVm. |callback| is called if the arguments are bad,
  // or after the method call finishes.
  void StopVm(std::string name, StopVmCallback callback);

  // Checks the arguments for starting a Container inside an existing Termina
  // VM. Starts the container via ConciergeClient::StartContainer. |callback|
  // is called if the arguments are bad, or after the method call finishes.
  void StartContainer(std::string vm_name,
                      std::string container_name,
                      std::string container_username,
                      StartContainerCallback callback);

  // Returns the singleton instance of CrostiniManager.
  static CrostiniManager* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<CrostiniManager>;

  CrostiniManager();
  ~CrostiniManager();

  // Callback for ConciergeClient::CreateDiskImage. Called after the Concierge
  // service method finishes.
  void OnCreateDiskImage(
      CreateDiskImageCallback callback,
      base::Optional<vm_tools::concierge::CreateDiskImageResponse> response);

  // Callback for ConciergeClient::StartTerminaVm. Called after the Concierge
  // service method finishes.
  void OnStartTerminaVm(
      StartTerminaVmCallback callback,
      base::Optional<vm_tools::concierge::StartVmResponse> response);

  // Callback for ConciergeClient::StopVm. Called after the Concierge
  // service method finishes.
  void OnStopVm(StopVmCallback callback,
                base::Optional<vm_tools::concierge::StopVmResponse> response);

  // Callback for CrostiniClient::StartVmConcierge. Called after the
  // DebugDaemon service method finishes.
  void OnStartVmConcierge(StartVmConciergeCallback callback, bool success);

  // Callback for CrostiniManager::StartContainer. Called after the Concierge
  // service finishes.
  void OnStartContainer(
      StartContainerCallback callback,
      base::Optional<vm_tools::concierge::StartContainerResponse> response);

  // Helper for CrostiniManager::CreateDiskImage. Separated so it can be run
  // off the main thread.
  void CreateDiskImageAfterSizeCheck(
      vm_tools::concierge::CreateDiskImageRequest request,
      CreateDiskImageCallback callback,
      int64_t free_disk_size);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrostiniManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniManager);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_MANAGER_H_
