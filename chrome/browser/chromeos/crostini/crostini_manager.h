// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_MANAGER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chromeos/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/dbus/cicerone_client.h"
#include "chromeos/dbus/concierge/service.pb.h"
#include "chromeos/dbus/concierge_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/device/public/mojom/usb_manager.mojom.h"

class Profile;

namespace crostini {

// Result types for various callbacks etc.

// WARNING: Do not remove or re-order these values, as they are used in user
// visible error messages and logs. New entries should only be added to the end.
// This message was added during development of M74, error codes from prior
// versions may differ from the numbering here.
enum class CrostiniResult {
  SUCCESS = 0,
  // DBUS_ERROR = 1,
  // UNPARSEABLE_RESPONSE = 2,
  // INSUFFICIENT_DISK = 3,
  CREATE_DISK_IMAGE_FAILED = 4,
  VM_START_FAILED = 5,
  VM_STOP_FAILED = 6,
  DESTROY_DISK_IMAGE_FAILED = 7,
  LIST_VM_DISKS_FAILED = 8,
  CLIENT_ERROR = 9,
  // DISK_TYPE_ERROR = 10,
  CONTAINER_DOWNLOAD_TIMED_OUT = 11,
  CONTAINER_CREATE_CANCELLED = 12,
  CONTAINER_CREATE_FAILED = 13,
  CONTAINER_START_CANCELLED = 14,
  CONTAINER_START_FAILED = 15,
  // LAUNCH_CONTAINER_APPLICATION_FAILED = 16,
  INSTALL_LINUX_PACKAGE_FAILED = 17,
  BLOCKING_OPERATION_ALREADY_ACTIVE = 18,
  UNINSTALL_PACKAGE_FAILED = 19,
  // SSHFS_MOUNT_ERROR = 20,
  OFFLINE_WHEN_UPGRADE_REQUIRED = 21,
  LOAD_COMPONENT_FAILED = 22,
  // PERMISSION_BROKER_ERROR = 23,
  // ATTACH_USB_FAILED = 24,
  // DETACH_USB_FAILED = 25,
  // LIST_USB_FAILED = 26,
  CROSTINI_UNINSTALLER_RUNNING = 27,
  // UNKNOWN_USB_DEVICE = 28,
  UNKNOWN_ERROR = 29,
  CONTAINER_EXPORT_IMPORT_FAILED = 30,
  CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED = 31,
  CONTAINER_EXPORT_IMPORT_FAILED_VM_STARTED = 32,
  CONTAINER_EXPORT_IMPORT_FAILED_ARCHITECTURE = 33,
  NOT_ALLOWED = 34,
  CONTAINER_EXPORT_IMPORT_FAILED_SPACE = 35,
  GET_CONTAINER_SSH_KEYS_FAILED = 36,
};

enum class InstallLinuxPackageProgressStatus {
  SUCCEEDED,
  FAILED,
  DOWNLOADING,
  INSTALLING,
};

enum class VmState {
  STARTING,
  STARTED,
  STOPPING,
};

enum class UninstallPackageProgressStatus {
  SUCCEEDED,
  FAILED,
  UNINSTALLING,  // In progress
};

enum class ExportContainerProgressStatus {
  PACK,
  DOWNLOAD,
};

enum class ImportContainerProgressStatus {
  UPLOAD,
  UNPACK,
  FAILURE_ARCHITECTURE,
  FAILURE_SPACE,
};

struct VmInfo {
  VmState state;
  vm_tools::concierge::VmInfo info;
};

struct ContainerInfo {
  ContainerInfo(std::string name, std::string username, std::string homedir);
  ~ContainerInfo();
  ContainerInfo(const ContainerInfo&);

  std::string name;
  std::string username;
  base::FilePath homedir;
  bool sshfs_mounted = false;
};

// Return type when getting app icons from within a container.
struct Icon {
  std::string desktop_file_id;

  // Icon file content in PNG format.
  std::string content;
};

struct LinuxPackageInfo {
  LinuxPackageInfo();
  LinuxPackageInfo(const LinuxPackageInfo&);
  ~LinuxPackageInfo();

  bool success;

  // A textual reason for the failure, only set when success is false.
  std::string failure_reason;

  // The remaining fields are only set when success is true.
  // package_id is given as "name;version;arch;data".
  std::string package_id;
  std::string name;
  std::string version;
  std::string summary;
  std::string description;
};

class LinuxPackageOperationProgressObserver {
 public:
  // A successfully started package install will continually fire progress
  // events until it returns a status of SUCCEEDED or FAILED. The
  // |progress_percent| field is given as a percentage of the given step,
  // DOWNLOADING or INSTALLING.
  virtual void OnInstallLinuxPackageProgress(
      const std::string& vm_name,
      const std::string& container_name,
      InstallLinuxPackageProgressStatus status,
      int progress_percent) = 0;

  // A successfully started package uninstall will continually fire progress
  // events until it returns a status of SUCCEEDED or FAILED.
  virtual void OnUninstallPackageProgress(const std::string& vm_name,
                                          const std::string& container_name,
                                          UninstallPackageProgressStatus status,
                                          int progress_percent) = 0;
};

class PendingAppListUpdatesObserver : public base::CheckedObserver {
 public:
  // Called whenever the kPendingAppListUpdatesMethod signal is sent.
  virtual void OnPendingAppListUpdates(const std::string& vm_name,
                                       const std::string& container_name,
                                       int count) = 0;
};

class ExportContainerProgressObserver {
 public:
  // A successfully started container export will continually fire progress
  // events until the original callback from ExportLxdContainer is invoked with
  // a status of SUCCESS or CONTAINER_EXPORT_FAILED.
  virtual void OnExportContainerProgress(const std::string& vm_name,
                                         const std::string& container_name,
                                         ExportContainerProgressStatus status,
                                         int progress_percent,
                                         uint64_t progress_speed) = 0;
};

class ImportContainerProgressObserver {
 public:
  // A successfully started container import will continually fire progress
  // events until the original callback from ImportLxdContainer is invoked with
  // a status of SUCCESS or CONTAINER_IMPORT_FAILED[_*].
  virtual void OnImportContainerProgress(
      const std::string& vm_name,
      const std::string& container_name,
      ImportContainerProgressStatus status,
      int progress_percent,
      uint64_t progress_speed,
      const std::string& architecture_device,
      const std::string& architecture_container,
      uint64_t available_space,
      uint64_t minimum_required_space) = 0;
};

class InstallerViewStatusObserver : public base::CheckedObserver {
 public:
  // Called when the CrostiniInstallerView is opened or closed.
  virtual void OnCrostiniInstallerViewStatusChanged(bool open) = 0;
};

class VmShutdownObserver : public base::CheckedObserver {
 public:
  // Called when the given VM has shutdown.
  virtual void OnVmShutdown(const std::string& vm_name) = 0;
};

// CrostiniManager is a singleton which is used to check arguments for
// ConciergeClient and CiceroneClient. ConciergeClient is dedicated to
// communication with the Concierge service, CiceroneClient is dedicated to
// communication with the Cicerone service and both should remain as thin as
// possible. The existence of Cicerone is abstracted behind this class and
// only the Concierge name is exposed outside of here.
class CrostiniManager : public KeyedService,
                        public chromeos::ConciergeClient::ContainerObserver,
                        public chromeos::CiceroneClient::Observer {
 public:
  using CrostiniResultCallback =
      base::OnceCallback<void(CrostiniResult result)>;
  // Callback indicating success or failure
  using BoolCallback = base::OnceCallback<void(bool success)>;

  // Observer class for the Crostini restart flow.
  class RestartObserver {
   public:
    virtual ~RestartObserver() {}
    virtual void OnComponentLoaded(CrostiniResult result) = 0;
    virtual void OnConciergeStarted(bool success) = 0;
    virtual void OnDiskImageCreated(bool success,
                                    vm_tools::concierge::DiskImageStatus status,
                                    int64_t disk_size_available) = 0;
    virtual void OnVmStarted(bool success) = 0;
    virtual void OnContainerDownloading(int32_t download_percent) = 0;
    virtual void OnContainerCreated(CrostiniResult result) = 0;
    virtual void OnContainerSetup(bool success) = 0;
    virtual void OnContainerStarted(CrostiniResult result) = 0;
    virtual void OnSshKeysFetched(bool success) = 0;
  };

  static CrostiniManager* GetForProfile(Profile* profile);

  explicit CrostiniManager(Profile* profile);
  ~CrostiniManager() override;

  // Returns true if the cros-termina component is installed.
  static bool IsCrosTerminaInstalled();

  // Returns true if the /dev/kvm directory is present.
  static bool IsDevKvmPresent();

  // Generate the URL for Crostini terminal application.
  static GURL GenerateVshInCroshUrl(
      Profile* profile,
      const std::string& vm_name,
      const std::string& container_name,
      const std::vector<std::string>& terminal_args);

  // Generate AppLaunchParams for the Crostini terminal application.
  static AppLaunchParams GenerateTerminalAppLaunchParams(Profile* profile);

  // Upgrades cros-termina component if the current version is not compatible.
  void MaybeUpgradeCrostini();

  // Installs the current version of cros-termina component. Attempts to apply
  // pending upgrades if a MaybeUpgradeCrostini failed.
  void InstallTerminaComponent(CrostiniResultCallback callback);

  // Unloads and removes the cros-termina component. Returns success/failure.
  bool UninstallTerminaComponent();

  // Starts the Concierge service. |callback| is called after the method call
  // finishes.
  void StartConcierge(BoolCallback callback);

  // Stops the Concierge service. |callback| is called after the method call
  // finishes.
  void StopConcierge(BoolCallback callback);

  // Checks the arguments for creating a new Termina VM disk image. Creates a
  // disk image for a Termina VM via ConciergeClient::CreateDiskImage.
  // |callback| is called if the arguments are bad, or after the method call
  // finishes.
  using CreateDiskImageCallback =
      base::OnceCallback<void(bool success,
                              vm_tools::concierge::DiskImageStatus,
                              const base::FilePath& disk_path)>;
  void CreateDiskImage(
      // The path to the disk image, including the name of
      // the image itself. The image name should match the
      // name of the VM that it will be used for.
      const base::FilePath& disk_path,
      // The storage location for the disk image
      vm_tools::concierge::StorageLocation storage_location,
      // The logical size of the disk image, in bytes
      int64_t disk_size_bytes,
      CreateDiskImageCallback callback);

  // Checks the arguments for destroying a named Termina VM disk image.
  // Removes the named Termina VM via ConciergeClient::DestroyDiskImage.
  // |callback| is called if the arguments are bad, or after the method call
  // finishes.
  void DestroyDiskImage(
      // The path to the disk image, including the name of the image itself.
      const base::FilePath& disk_path,
      BoolCallback callback);

  using ListVmDisksCallback =
      base::OnceCallback<void(CrostiniResult result, int64_t total_size)>;
  void ListVmDisks(ListVmDisksCallback callback);

  // Checks the arguments for starting a Termina VM. Starts a Termina VM via
  // ConciergeClient::StartTerminaVm. |callback| is called if the arguments
  // are bad, or after the method call finishes.
  void StartTerminaVm(
      // The human-readable name to be assigned to this VM.
      std::string name,
      // Path to the disk image on the host.
      const base::FilePath& disk_path,
      BoolCallback callback);

  // Checks the arguments for stopping a Termina VM. Stops the Termina VM via
  // ConciergeClient::StopVm. |callback| is called if the arguments are bad,
  // or after the method call finishes.
  void StopVm(std::string name, CrostiniResultCallback callback);

  // Checks the arguments for creating an Lxd container via
  // CiceroneClient::CreateLxdContainer. |callback| is called immediately if the
  // arguments are bad, or once the container has been created.
  void CreateLxdContainer(std::string vm_name,
                          std::string container_name,
                          CrostiniResultCallback callback);

  // Checks the arguments for deleting an Lxd container via
  // CiceroneClient::DeleteLxdContainer. |callback| is called immediately if the
  // arguments are bad, or once the container has been deleted.
  void DeleteLxdContainer(std::string vm_name,
                          std::string container_name,
                          BoolCallback callback);

  // Checks the arguments for starting an Lxd container via
  // CiceroneClient::StartLxdContainer. |callback| is called immediately if the
  // arguments are bad, or once the container has been created.
  void StartLxdContainer(std::string vm_name,
                         std::string container_name,
                         CrostiniResultCallback callback);

  // Checks the arguments for setting up an Lxd container user via
  // CiceroneClient::SetUpLxdContainerUser. |callback| is called immediately if
  // the arguments are bad, or once garcon has been started.
  void SetUpLxdContainerUser(std::string vm_name,
                             std::string container_name,
                             std::string container_username,
                             BoolCallback callback);

  // Checks the arguments for exporting an Lxd container via
  // CiceroneClient::ExportLxdContainer. |callback| is called immediately if the
  // arguments are bad, or after the method call finishes.
  void ExportLxdContainer(std::string vm_name,
                          std::string container_name,
                          base::FilePath export_path,
                          CrostiniResultCallback callback);

  // Checks the arguments for importing an Lxd container via
  // CiceroneClient::ImportLxdContainer. |callback| is called immediately if the
  // arguments are bad, or after the method call finishes.
  void ImportLxdContainer(std::string vm_name,
                          std::string container_name,
                          base::FilePath import_path,
                          CrostiniResultCallback callback);

  // Asynchronously launches an app as specified by its desktop file id.
  void LaunchContainerApplication(std::string vm_name,
                                  std::string container_name,
                                  std::string desktop_file_id,
                                  const std::vector<std::string>& files,
                                  bool display_scaled,
                                  BoolCallback callback);

  // Asynchronously gets app icons as specified by their desktop file ids.
  // |callback| is called after the method call finishes.
  using GetContainerAppIconsCallback =
      base::OnceCallback<void(bool success, const std::vector<Icon>& icons)>;
  void GetContainerAppIcons(std::string vm_name,
                            std::string container_name,
                            std::vector<std::string> desktop_file_ids,
                            int icon_size,
                            int scale,
                            GetContainerAppIconsCallback callback);

  // Asynchronously retrieve information about a Linux Package (.deb) inside the
  // container.
  using GetLinuxPackageInfoCallback =
      base::OnceCallback<void(const LinuxPackageInfo&)>;
  void GetLinuxPackageInfo(Profile* profile,
                           std::string vm_name,
                           std::string container_name,
                           std::string package_path,
                           GetLinuxPackageInfoCallback callback);

  // Asynchronously retrieve information about a Linux Package in the APT
  // repository. This uses a package_name to identify a package.
  void GetLinuxPackageInfoFromApt(const std::string& vm_name,
                                  const std::string& container_name,
                                  const std::string& package_name,
                                  GetLinuxPackageInfoCallback callback);

  // Begin installation of a Linux Package inside the container. If the
  // installation is successfully started, further updates will be sent to
  // added LinuxPackageOperationProgressObservers.
  using InstallLinuxPackageCallback = CrostiniResultCallback;
  void InstallLinuxPackage(std::string vm_name,
                           std::string container_name,
                           std::string package_path,
                           InstallLinuxPackageCallback callback);

  // Begin installation of a Linux Package inside the container. If the
  // installation is successfully started, further updates will be sent to
  // added LinuxPackageOperationProgressObservers. Uses a package_id, given
  // by "package_name;version;arch;data", to identify the package to install
  // from the APT repository.
  void InstallLinuxPackageFromApt(const std::string& vm_name,
                                  const std::string& container_name,
                                  const std::string& package_id,
                                  InstallLinuxPackageCallback callback);

  // Begin uninstallation of a Linux Package inside the container. The package
  // is identified by its associated .desktop file's ID; we don't use package_id
  // to avoid problems with stale package_ids (such as after upgrades). If the
  // uninstallation is successfully started, further updates will be sent to
  // added LinuxPackageOperationProgressObservers.
  void UninstallPackageOwningFile(std::string vm_name,
                                  std::string container_name,
                                  std::string desktop_file_id,
                                  CrostiniResultCallback callback);

  // Asynchronously gets SSH server public key of container and trusted SSH
  // client private key which can be used to connect to the container.
  // |callback| is called after the method call finishes.
  using GetContainerSshKeysCallback =
      base::OnceCallback<void(bool success,
                              const std::string& container_public_key,
                              const std::string& host_private_key,
                              const std::string& hostname)>;
  void GetContainerSshKeys(std::string vm_name,
                           std::string container_name,
                           GetContainerSshKeysCallback callback);

  // Called when a USB device should be attached into the VM. Should only ever
  // be called on user action. The guest_port is only valid on success.
  using AttachUsbDeviceCallback =
      base::OnceCallback<void(bool success, uint8_t guest_port)>;
  void AttachUsbDevice(const std::string& vm_name,
                       device::mojom::UsbDeviceInfoPtr device,
                       base::ScopedFD fd,
                       AttachUsbDeviceCallback callback);

  // Called when a USB device should be detached from the VM.
  // May be called on user action or on USB removal.
  void DetachUsbDevice(const std::string& vm_name,
                       device::mojom::UsbDeviceInfoPtr device,
                       uint8_t guest_port,
                       BoolCallback callback);

  // Lists USB devices attached to a guest VM.
  // TODO(jopra): Rename to reflect that this now lists the mount points for USB
  // devices.
  using ListUsbDevicesCallback = base::OnceCallback<
      void(bool success, std::vector<std::pair<std::string, uint8_t>> devices)>;
  void ListUsbDevices(const std::string& vm_name,
                      ListUsbDevicesCallback callback);

  // Create the crosh-in-a-window that displays a shell in an container on a VM.
  static Browser* CreateContainerTerminal(const AppLaunchParams& launch_params,
                                          const GURL& vsh_in_crosh_url);

  // Shows the already created crosh-in-a-window that displays a shell in an
  // already running container on a VM.
  static void ShowContainerTerminal(const AppLaunchParams& launch_params,
                                    const GURL& vsh_in_crosh_url,
                                    Browser* browser);

  // Launches the crosh-in-a-window that displays a shell in an already running
  // container on a VM and passes |terminal_args| as parameters to that shell
  // which will cause them to be executed as program inside that shell.
  void LaunchContainerTerminal(const std::string& vm_name,
                               const std::string& container_name,
                               const std::vector<std::string>& terminal_args);

  // Searches for not installed packages that have names matching the passed
  // plaintext search query and returns a vector containing their names.
  using SearchAppCallback =
      base::OnceCallback<void(const std::vector<std::string>& package_names)>;
  void SearchApp(const std::string& vm_name,
                 const std::string& container_name,
                 const std::string& query,
                 SearchAppCallback callback);

  using RestartId = int;
  static const RestartId kUninitializedRestartId = -1;
  // Runs all the steps required to restart the given crostini vm and container.
  // The optional |observer| tracks progress.
  RestartId RestartCrostini(std::string vm_name,
                            std::string container_name,
                            CrostiniResultCallback callback,
                            RestartObserver* observer = nullptr);

  // Aborts a restart. A "next" restarter with the same <vm_name,
  // container_name> will run, if there is one. |callback| will be called once
  // the restart has finished aborting
  void AbortRestartCrostini(RestartId restart_id, base::OnceClosure callback);

  // Returns true if the Restart corresponding to |restart_id| is not yet
  // complete.
  bool IsRestartPending(RestartId restart_id);

  // Adds a callback to receive notification of container shutdown.
  void AddShutdownContainerCallback(std::string vm_name,
                                    std::string container_name,
                                    base::OnceClosure shutdown_callback);

  // Adds a callback to receive uninstall notification.
  using RemoveCrostiniCallback = CrostiniResultCallback;
  void AddRemoveCrostiniCallback(RemoveCrostiniCallback remove_callback);

  // Add/remove observers for package install and uninstall progress.
  void AddLinuxPackageOperationProgressObserver(
      LinuxPackageOperationProgressObserver* observer);
  void RemoveLinuxPackageOperationProgressObserver(
      LinuxPackageOperationProgressObserver* observer);

  // Add/remove observers for pending app list updates.
  void AddPendingAppListUpdatesObserver(
      PendingAppListUpdatesObserver* observer);
  void RemovePendingAppListUpdatesObserver(
      PendingAppListUpdatesObserver* observer);

  // Add/remove observers for container export/import.
  void AddExportContainerProgressObserver(
      ExportContainerProgressObserver* observer);
  void RemoveExportContainerProgressObserver(
      ExportContainerProgressObserver* observer);
  void AddImportContainerProgressObserver(
      ImportContainerProgressObserver* observer);
  void RemoveImportContainerProgressObserver(
      ImportContainerProgressObserver* observer);

  // Add/remove vm shutdown observers.
  void AddVmShutdownObserver(VmShutdownObserver* observer);
  void RemoveVmShutdownObserver(VmShutdownObserver* observer);

  // ConciergeClient::Observer:
  void OnContainerStartupFailed(
      const vm_tools::concierge::ContainerStartedSignal& signal) override;

  // CiceroneClient::Observer:
  void OnContainerStarted(
      const vm_tools::cicerone::ContainerStartedSignal& signal) override;
  void OnContainerShutdown(
      const vm_tools::cicerone::ContainerShutdownSignal& signal) override;
  void OnInstallLinuxPackageProgress(
      const vm_tools::cicerone::InstallLinuxPackageProgressSignal& signal)
      override;
  void OnUninstallPackageProgress(
      const vm_tools::cicerone::UninstallPackageProgressSignal& signal)
      override;
  void OnLxdContainerCreated(
      const vm_tools::cicerone::LxdContainerCreatedSignal& signal) override;
  void OnLxdContainerDeleted(
      const vm_tools::cicerone::LxdContainerDeletedSignal& signal) override;
  void OnLxdContainerDownloading(
      const vm_tools::cicerone::LxdContainerDownloadingSignal& signal) override;
  void OnTremplinStarted(
      const vm_tools::cicerone::TremplinStartedSignal& signal) override;
  void OnLxdContainerStarting(
      const vm_tools::cicerone::LxdContainerStartingSignal& signal) override;
  void OnExportLxdContainerProgress(
      const vm_tools::cicerone::ExportLxdContainerProgressSignal& signal)
      override;
  void OnImportLxdContainerProgress(
      const vm_tools::cicerone::ImportLxdContainerProgressSignal& signal)
      override;
  void OnPendingAppListUpdates(
      const vm_tools::cicerone::PendingAppListUpdatesSignal& signal) override;

  void RemoveCrostini(std::string vm_name, RemoveCrostiniCallback callback);

  void UpdateVmState(std::string vm_name, VmState vm_state);
  bool IsVmRunning(std::string vm_name);
  // Returns null if VM is not running.
  base::Optional<VmInfo> GetVmInfo(std::string vm_name);
  void AddRunningVmForTesting(std::string vm_name);

  void SetContainerSshfsMounted(std::string vm_name,
                                std::string container_name);
  // Returns null if VM or container is not running.
  base::Optional<ContainerInfo> GetContainerInfo(std::string vm_name,
                                                 std::string container_name);
  void AddRunningContainerForTesting(std::string vm_name, ContainerInfo info);

  // If the Crostini reporting policy is set, save the last app launch
  // time window and the Termina version in prefs for asynchronous reporting.
  void UpdateLaunchMetricsForEnterpriseReporting();

  // Clear the lists of running VMs and containers.
  // Can be called for testing to skip restart.
  void set_skip_restart_for_testing() { skip_restart_for_testing_ = true; }
  bool skip_restart_for_testing() { return skip_restart_for_testing_; }
  void set_component_manager_load_error_for_testing(
      component_updater::CrOSComponentManager::Error error) {
    component_manager_load_error_for_testing_ = error;
  }

  void SetInstallerViewStatus(bool open);
  bool GetInstallerViewStatus() const;
  void AddInstallerViewStatusObserver(InstallerViewStatusObserver* observer);
  void RemoveInstallerViewStatusObserver(InstallerViewStatusObserver* observer);
  bool HasInstallerViewStatusObserver(InstallerViewStatusObserver* observer);

  void OnDBusShuttingDownForTesting();

 private:
  class CrostiniRestarter;

  void RemoveDBusObservers();

  // Callback for ConciergeClient::CreateDiskImage. Called after the Concierge
  // service method finishes.
  void OnCreateDiskImage(
      CreateDiskImageCallback callback,
      base::Optional<vm_tools::concierge::CreateDiskImageResponse> reply);

  // Callback for ConciergeClient::DestroyDiskImage. Called after the Concierge
  // service method finishes.
  void OnDestroyDiskImage(
      BoolCallback callback,
      base::Optional<vm_tools::concierge::DestroyDiskImageResponse> reply);

  // Callback for ConciergeClient::ListVmDisks. Called after the Concierge
  // service method finishes.
  void OnListVmDisks(
      ListVmDisksCallback callback,
      base::Optional<vm_tools::concierge::ListVmDisksResponse> reply);

  // Callback for ConciergeClient::StartTerminaVm. Called after the Concierge
  // service method finishes.  Updates running containers list then calls the
  // |callback| if the container has already been started, otherwise passes the
  // callback to OnStartTremplin.
  void OnStartTerminaVm(
      std::string vm_name,
      BoolCallback callback,
      base::Optional<vm_tools::concierge::StartVmResponse> reply);

  // Callback for ConciergeClient::TremplinStartedSignal. Called after the
  // Tremplin service starts. Updates running containers list and then calls the
  // |callback| with true, indicating success.
  void OnStartTremplin(std::string vm_name, BoolCallback callback);

  // Callback for ConciergeClient::StopVm. Called after the Concierge
  // service method finishes.
  void OnStopVm(std::string vm_name,
                CrostiniResultCallback callback,
                base::Optional<vm_tools::concierge::StopVmResponse> reply);

  // Callback for CrostiniManager::InstallCrostiniComponent. Must be called on
  // the UI thread.
  void OnInstallTerminaComponent(
      CrostiniResultCallback callback,
      bool is_update_checked,
      component_updater::CrOSComponentManager::Error error,
      const base::FilePath& result);

  // Callback for CrostiniClient::StartConcierge. Called after the
  // DebugDaemon service method finishes.
  void OnStartConcierge(BoolCallback callback, bool success);

  // Callback for CrostiniClient::StopConcierge. Called after the
  // DebugDaemon service method finishes.
  void OnStopConcierge(BoolCallback callback, bool success);

  // Callback for CiceroneClient::CreateLxdContainer. May indicate the container
  // is still being created, in which case we will wait for an
  // OnLxdContainerCreated event.
  void OnCreateLxdContainer(
      std::string vm_name,
      std::string container_name,
      CrostiniResultCallback callback,
      base::Optional<vm_tools::cicerone::CreateLxdContainerResponse> reply);

  // Callback for CiceroneClient::DeleteLxdContainer.
  void OnDeleteLxdContainer(
      std::string vm_name,
      std::string container_name,
      BoolCallback callback,
      base::Optional<vm_tools::cicerone::DeleteLxdContainerResponse> reply);

  // Callback for CiceroneClient::StartLxdContainer.
  void OnStartLxdContainer(
      std::string vm_name,
      std::string container_name,
      CrostiniResultCallback callback,
      base::Optional<vm_tools::cicerone::StartLxdContainerResponse> reply);

  // Callback for CiceroneClient::SetUpLxdContainerUser.
  void OnSetUpLxdContainerUser(
      std::string vm_name,
      std::string container_name,
      BoolCallback callback,
      base::Optional<vm_tools::cicerone::SetUpLxdContainerUserResponse> reply);

  // Callback for CiceroneClient::ExportLxdContainer.
  void OnExportLxdContainer(
      std::string vm_name,
      std::string container_name,
      base::Optional<vm_tools::cicerone::ExportLxdContainerResponse> reply);

  // Callback for CiceroneClient::ImportLxdContainer.
  void OnImportLxdContainer(
      std::string vm_name,
      std::string container_name,
      base::Optional<vm_tools::cicerone::ImportLxdContainerResponse> reply);

  // Callback for CrostiniManager::LaunchContainerApplication.
  void OnLaunchContainerApplication(
      BoolCallback callback,
      base::Optional<vm_tools::cicerone::LaunchContainerApplicationResponse>
          reply);

  // Callback for CrostiniManager::GetContainerAppIcons. Called after the
  // Concierge service finishes.
  void OnGetContainerAppIcons(
      GetContainerAppIconsCallback callback,
      base::Optional<vm_tools::cicerone::ContainerAppIconResponse> reply);

  // Callback for CrostiniManager::GetLinuxPackageInfo and
  // CrostiniManager::GetLinuxPackageInfoFromApt.
  void OnGetLinuxPackageInfo(
      GetLinuxPackageInfoCallback callback,
      base::Optional<vm_tools::cicerone::LinuxPackageInfoResponse> reply);

  // Callback for CrostiniManager::InstallLinuxPackage.
  void OnInstallLinuxPackage(
      InstallLinuxPackageCallback callback,
      base::Optional<vm_tools::cicerone::InstallLinuxPackageResponse> reply);

  // Callback for CrostiniManager::UninstallPackageOwningFile.
  void OnUninstallPackageOwningFile(
      CrostiniResultCallback callback,
      base::Optional<vm_tools::cicerone::UninstallPackageOwningFileResponse>
          reply);

  // Callback for CrostiniManager::GetContainerSshKeys. Called after the
  // Concierge service finishes.
  void OnGetContainerSshKeys(
      GetContainerSshKeysCallback callback,
      base::Optional<vm_tools::concierge::ContainerSshKeysResponse> reply);

  // Callback for CrostiniManager::OnAttachUsbDeviceOpen
  void OnAttachUsbDevice(
      const std::string& vm_name,
      device::mojom::UsbDeviceInfoPtr device,
      AttachUsbDeviceCallback callback,
      base::Optional<vm_tools::concierge::AttachUsbDeviceResponse> reply);

  // Callback for CrostiniManager::DetachUsbDevice
  void OnDetachUsbDevice(
      const std::string& vm_name,
      uint8_t guest_port,
      device::mojom::UsbDeviceInfoPtr device,
      BoolCallback callback,
      base::Optional<vm_tools::concierge::DetachUsbDeviceResponse> reply);

  // Callback for CrostiniManager::ListUsbDevices
  void OnListUsbDevices(
      const std::string& vm_name,
      ListUsbDevicesCallback callback,
      base::Optional<vm_tools::concierge::ListUsbDeviceResponse> reply);

  // Callback for CrostiniManager::SearchApp.
  void OnSearchApp(SearchAppCallback callback,
                   base::Optional<vm_tools::cicerone::AppSearchResponse> reply);

  // Helper for CrostiniManager::MaybeUpgradeCrostini. Makes blocking calls to
  // check for file paths and registered components.
  static void CheckPathsAndComponents();

  // Helper for CrostiniManager::MaybeUpgradeCrostini. Separated because the
  // checking component registration code may block.
  void MaybeUpgradeCrostiniAfterChecks();

  void FinishRestart(CrostiniRestarter* restarter, CrostiniResult result);

  // Callback for CrostiniManager::AbortRestartCrostini
  void OnAbortRestartCrostini(RestartId restart_id, base::OnceClosure callback);

  // Callback for CrostiniManager::RemoveCrostini.
  void OnRemoveCrostini(CrostiniResult result);

  Profile* profile_;
  std::string owner_id_;

  bool skip_restart_for_testing_ = false;
  component_updater::CrOSComponentManager::Error
      component_manager_load_error_for_testing_ =
          component_updater::CrOSComponentManager::Error::NONE;

  static bool is_cros_termina_registered_;
  bool termina_update_check_needed_ = false;
  static bool is_dev_kvm_present_;

  // Callbacks that are waiting on a signal
  std::multimap<ContainerId, CrostiniResultCallback> start_container_callbacks_;
  std::multimap<ContainerId, base::OnceClosure> shutdown_container_callbacks_;
  std::multimap<ContainerId, CrostiniResultCallback>
      create_lxd_container_callbacks_;
  std::multimap<ContainerId, BoolCallback> delete_lxd_container_callbacks_;
  std::map<ContainerId, CrostiniResultCallback> export_lxd_container_callbacks_;
  std::map<ContainerId, CrostiniResultCallback> import_lxd_container_callbacks_;

  // Callbacks to run after Tremplin is started, keyed by vm_name. These are
  // used if StartTerminaVm completes but we need to wait from Tremplin to
  // start.
  std::multimap<std::string, base::OnceClosure> tremplin_started_callbacks_;

  std::map<std::string, VmInfo> running_vms_;

  // Running containers as keyed by vm name.
  std::multimap<std::string, ContainerInfo> running_containers_;

  std::vector<RemoveCrostiniCallback> remove_crostini_callbacks_;

  base::ObserverList<LinuxPackageOperationProgressObserver>::Unchecked
      linux_package_operation_progress_observers_;

  base::ObserverList<PendingAppListUpdatesObserver>
      pending_app_list_updates_observers_;

  base::ObserverList<ExportContainerProgressObserver>::Unchecked
      export_container_progress_observers_;
  base::ObserverList<ImportContainerProgressObserver>::Unchecked
      import_container_progress_observers_;

  base::ObserverList<VmShutdownObserver> vm_shutdown_observers_;

  // Only one restarter flow is actually running for a given container, other
  // restarters will just have their callback called when the running restarter
  // completes.
  std::multimap<ContainerId, CrostiniManager::RestartId>
      restarters_by_container_;

  std::map<CrostiniManager::RestartId, scoped_refptr<CrostiniRestarter>>
      restarters_by_id_;

  // True when the installer dialog is showing. At that point, it is invalid
  // to allow Crostini uninstallation.
  bool installer_dialog_showing_ = false;

  base::ObserverList<InstallerViewStatusObserver>
      installer_view_status_observers_;

  bool dbus_observers_removed_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrostiniManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniManager);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_MANAGER_H_
