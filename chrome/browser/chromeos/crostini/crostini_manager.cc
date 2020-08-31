// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_manager.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/crostini/ansible/ansible_management_service.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_port_forwarder.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_remover.h"
#include "chrome/browser/chromeos/crostini/crostini_reporting_util.h"
#include "chrome/browser/chromeos/crostini/crostini_types.mojom.h"
#include "chrome/browser/chromeos/crostini/throttle/crostini_throttle.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/chromeos/policy/powerwash_requirements_checker.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/scheduler_configuration_manager.h"
#include "chrome/browser/chromeos/usb/cros_usb_detector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/anomaly_detector_client.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/image_loader_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/component_updater/component_updater_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "dbus/message.h"
#include "extensions/browser/extension_registry.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/base/window_open_disposition.h"

namespace crostini {

namespace {

chromeos::CiceroneClient* GetCiceroneClient() {
  return chromeos::DBusThreadManager::Get()->GetCiceroneClient();
}

chromeos::ConciergeClient* GetConciergeClient() {
  return chromeos::DBusThreadManager::Get()->GetConciergeClient();
}

chromeos::AnomalyDetectorClient* GetAnomalyDetectorClient() {
  return chromeos::DBusThreadManager::Get()->GetAnomalyDetectorClient();
}

// Find any callbacks for the specified |vm_name|, invoke them with
// |arguments|... and erase them from the map.
template <typename... Parameters, typename... Arguments>
void InvokeAndErasePendingCallbacks(
    std::map<ContainerId, base::OnceCallback<void(Parameters...)>>*
        vm_keyed_map,
    const std::string& vm_name,
    Arguments&&... arguments) {
  for (auto it = vm_keyed_map->begin(); it != vm_keyed_map->end();) {
    if (it->first.vm_name == vm_name) {
      std::move(it->second).Run(arguments...);
      vm_keyed_map->erase(it++);
    } else {
      ++it;
    }
  }
}

// Find any container callbacks for the specified |vm_name| and
// |container_name|, invoke them with |result| and erase them from the map.
void InvokeAndErasePendingContainerCallbacks(
    std::multimap<ContainerId, CrostiniManager::CrostiniResultCallback>*
        container_callbacks,
    const std::string& vm_name,
    const std::string& container_name,
    CrostiniResult result) {
  auto range =
      container_callbacks->equal_range(ContainerId(vm_name, container_name));
  for (auto it = range.first; it != range.second; ++it) {
    std::move(it->second).Run(result);
  }
  container_callbacks->erase(range.first, range.second);
}

bool IsMicSharingEnabled(Profile* profile, bool crostini_mic_sharing_enabled) {
  if (base::FeatureList::IsEnabled(
          chromeos::features::kCrostiniShowMicSetting) &&
      profile->GetPrefs()->GetBoolean(::prefs::kAudioCaptureAllowed) &&
      crostini_mic_sharing_enabled) {
    return true;
  }
  return false;
}

}  // namespace

CrostiniManager::RestartOptions::RestartOptions() = default;
CrostiniManager::RestartOptions::RestartOptions(RestartOptions&&) = default;
CrostiniManager::RestartOptions::~RestartOptions() = default;
CrostiniManager::RestartOptions& CrostiniManager::RestartOptions::operator=(
    RestartOptions&&) = default;

class CrostiniManager::CrostiniRestarter
    : public crostini::VmShutdownObserver,
      public chromeos::disks::DiskMountManager::Observer,
      public chromeos::SchedulerConfigurationManagerBase::Observer {
 public:
  CrostiniRestarter(Profile* profile,
                    CrostiniManager* crostini_manager,
                    std::string vm_name,
                    std::string container_name,
                    RestartOptions options,
                    CrostiniManager::CrostiniResultCallback callback)
      : profile_(profile),
        crostini_manager_(crostini_manager),
        vm_name_(std::move(vm_name)),
        container_name_(std::move(container_name)),
        options_(std::move(options)),
        completed_callback_(std::move(callback)),
        restart_id_(next_restart_id_++) {
  }

  void Restart() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!CrostiniFeatures::Get()->IsUIAllowed(profile_)) {
      LOG(ERROR) << "Crostini UI not allowed for profile "
                 << profile_->GetProfileUserName();
      std::move(completed_callback_).Run(CrostiniResult::NOT_ALLOWED);
      return;
    }

    crostini_manager_->AddVmShutdownObserver(this);

    StartStage(mojom::InstallerState::kStart);
    is_initial_install_ =
        crostini_manager_->GetCrostiniDialogStatus(DialogType::INSTALLER);
    if (ReturnEarlyIfAborted()) {
      return;
    }

    auto vm_info = crostini_manager_->GetVmInfo(vm_name_);
    // If vm is stopping, we wait until OnVmShutdown() to kick it off.
    if (vm_info && vm_info->state == VmState::STOPPING) {
      LOG(WARNING) << "Delay restart due to vm stopping";
    } else {
      ContinueRestart();
    }
  }

  void AddObserver(CrostiniManager::RestartObserver* observer) {
    observer->set_restart_id(restart_id_);
    observer_list_.AddObserver(observer);
  }

  void RunCallback(CrostiniResult result) {
    // Observer should not be called if we have completed.
    observer_list_.Clear();
    if (completed_callback_) {
      std::move(completed_callback_).Run(result);
    }
  }

  // crostini::VmShutdownObserver
  void OnVmShutdown(const std::string& vm_name) override {
    if (ReturnEarlyIfAborted()) {
      return;
    }
    if (vm_name == vm_name_) {
      if (is_running_) {
        LOG(WARNING) << "Unexpected VM shutdown during restart for " << vm_name;
        FinishRestart(CrostiniResult::RESTART_FAILED_VM_STOPPED);
      } else {
        // We can only get here if Restart() was called to register the shutdown
        // observer, and since is_running_ is false, we are waiting for this
        // shutdown to actually kick off the process.
        VLOG(1) << "resume restart on vm shutdown";
        base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                       base::BindOnce(&CrostiniRestarter::ContinueRestart,
                                      weak_ptr_factory_.GetWeakPtr()));
      }
    }
  }

  void Abort(base::OnceClosure callback) {
    is_aborted_ = true;
    observer_list_.Clear();
    abort_callbacks_.push_back(std::move(callback));
    result_ = CrostiniResult::RESTART_ABORTED;
    // Don't want to use FinishRestart here, because the next restarter in
    // line needs to run.
    RunCallback(result_);
  }

  // If this method returns true, then |this| may have been deleted and it is
  // unsafe to refer to any member variables.
  bool ReturnEarlyIfAborted() {
    if (is_aborted_ && !abort_callbacks_.empty()) {
      // The abort callbacks may delete this, so move the callback vector out of
      // the class.
      std::vector<base::OnceClosure> abort_callbacks_safe =
          std::move(abort_callbacks_);
      for (auto& abort_callback : abort_callbacks_safe) {
        std::move(abort_callback).Run();
      }
      // The abort callback may delete this, so it's not safe to
      // refer to |is_aborted_| after this point.
      return true;
    }
    return is_aborted_;
  }

  void OnContainerDownloading(int download_percent) {
    if (!is_running_) {
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnContainerDownloading(download_percent);
    }
  }

  CrostiniManager::RestartId restart_id() const { return restart_id_; }
  std::string vm_name() const { return vm_name_; }
  std::string container_name() const { return container_name_; }
  bool is_aborted() const { return is_aborted_; }
  CrostiniResult result_ = CrostiniResult::NEVER_FINISHED;

  ~CrostiniRestarter() override {
    // Do not record results if this restart was triggered by the installer.
    // The crostini installer has its own histograms that should be kept
    // separate.
    if (!is_initial_install_) {
      base::UmaHistogramEnumeration("Crostini.RestarterResult", result_);
    }
    crostini_manager_->RemoveVmShutdownObserver(this);
    if (completed_callback_) {
      LOG(ERROR) << "Destroying without having called the callback.";
    }
    auto* mount_manager = chromeos::disks::DiskMountManager::GetInstance();
    if (mount_manager)
      mount_manager->RemoveObserver(this);
  }

 private:
  void ContinueRestart() {
    is_running_ = true;
    // Skip to the end immediately if testing.
    if (crostini_manager_->skip_restart_for_testing()) {
      base::PostTask(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(&CrostiniRestarter::StartLxdContainerFinished,
                         weak_ptr_factory_.GetWeakPtr(),
                         CrostiniResult::SUCCESS));
      return;
    }

    StartStage(mojom::InstallerState::kInstallImageLoader);
    crostini_manager_->InstallTerminaComponent(
        base::BindOnce(&CrostiniRestarter::LoadComponentFinished,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void StartStage(mojom::InstallerState stage) {
    for (auto& observer : observer_list_) {
      observer.OnStageStarted(stage);
    }
  }

  void FinishRestart(CrostiniResult result) {
    DCHECK(!is_aborted_);

    // FinishRestart will delete this, so it's not safe to call any methods
    // after this point.
    crostini_manager_->FinishRestart(this, result);
  }

  void LoadComponentFinished(CrostiniResult result) {
    for (auto& observer : observer_list_) {
      observer.OnComponentLoaded(result);
    }
    if (ReturnEarlyIfAborted()) {
      return;
    }
    if (result != CrostiniResult::SUCCESS) {
      FinishRestart(result);
      return;
    }
    // Set the pref here, after we first successfully install something
    profile_->GetPrefs()->SetBoolean(crostini::prefs::kCrostiniEnabled, true);
    StartStage(mojom::InstallerState::kStartConcierge);
    crostini_manager_->StartConcierge(base::BindOnce(
        &CrostiniRestarter::ConciergeStarted, weak_ptr_factory_.GetWeakPtr()));
  }

  void ConciergeStarted(bool is_started) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    for (auto& observer : observer_list_) {
      observer.OnConciergeStarted(is_started);
    }
    if (ReturnEarlyIfAborted()) {
      return;
    }
    if (!is_started) {
      FinishRestart(CrostiniResult::CONCIERGE_START_FAILED);
      return;
    }

    // Allow concierge to choose an appropriate disk image size.
    int64_t disk_size_bytes = options_.disk_size_bytes.value_or(0);
    // If we have an already existing disk, CreateDiskImage will just return its
    // path so we can pass it to StartTerminaVm.
    StartStage(mojom::InstallerState::kCreateDiskImage);
    crostini_manager_->CreateDiskImage(
        base::FilePath(vm_name_),
        vm_tools::concierge::StorageLocation::STORAGE_CRYPTOHOME_ROOT,
        disk_size_bytes,
        base::BindOnce(&CrostiniRestarter::CreateDiskImageFinished,
                       weak_ptr_factory_.GetWeakPtr(), disk_size_bytes));
  }

  void CreateDiskImageFinished(int64_t disk_size_bytes,
                               bool success,
                               vm_tools::concierge::DiskImageStatus status,
                               const base::FilePath& result_path) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    for (auto& observer : observer_list_) {
      observer.OnDiskImageCreated(success, status, disk_size_bytes);
    }
    if (ReturnEarlyIfAborted()) {
      return;
    }
    if (!success) {
      FinishRestart(CrostiniResult::CREATE_DISK_IMAGE_FAILED);
      return;
    }
    disk_path_ = result_path;

    auto* scheduler_configuration_manager =
        g_browser_process->platform_part()->scheduler_configuration_manager();
    base::Optional<std::pair<bool, size_t>> scheduler_configuration =
        scheduler_configuration_manager->GetLastReply();
    if (!scheduler_configuration) {
      // Wait for the configuration to become available.
      LOG(WARNING) << "Scheduler configuration is not yet ready";
      scheduler_configuration_manager->AddObserver(this);
      return;
    }
    OnConfigurationSet(scheduler_configuration->first,
                       scheduler_configuration->second);
  }

  // chromeos::SchedulerConfigurationManagerBase::Observer:
  void OnConfigurationSet(bool success, size_t num_cores_disabled) override {
    // Note: On non-x86_64 devices, the configuration request to debugd always
    // fails. It is WAI, and to support that case, don't log anything even when
    // |success| is false. |num_cores_disabled| is always set regardless of
    // whether the call is successful.
    g_browser_process->platform_part()
        ->scheduler_configuration_manager()
        ->RemoveObserver(this);
    StartStage(mojom::InstallerState::kStartTerminaVm);
    crostini_manager_->StartTerminaVm(
        vm_name_, disk_path_, num_cores_disabled,
        base::BindOnce(&CrostiniRestarter::StartTerminaVmFinished,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void StartTerminaVmFinished(bool success) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    for (auto& observer : observer_list_) {
      observer.OnVmStarted(success);
    }
    if (ReturnEarlyIfAborted()) {
      return;
    }
    if (!success) {
      FinishRestart(CrostiniResult::VM_START_FAILED);
      return;
    }
    // Cache kernel version for enterprise reporting, if it is enabled
    // by policy, and we are in the default Termina/penguin case.
    if (profile_->GetPrefs()->GetBoolean(
            crostini::prefs::kReportCrostiniUsageEnabled) &&
        vm_name_ == kCrostiniDefaultVmName &&
        container_name_ == kCrostiniDefaultContainerName) {
      crostini_manager_->GetTerminaVmKernelVersion(
          base::BindOnce(&CrostiniRestarter::GetTerminaVmKernelVersionFinished,
                         weak_ptr_factory_.GetWeakPtr()));
    }
    StartStage(mojom::InstallerState::kCreateContainer);
    crostini_manager_->CreateLxdContainer(
        vm_name_, container_name_,
        base::BindOnce(&CrostiniRestarter::CreateLxdContainerFinished,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void GetTerminaVmKernelVersionFinished(
      const base::Optional<std::string>& maybe_kernel_version) {
    // In the error case, Crostini should still start, so we do not propagate
    // errors any further here. Also, any error would already have been logged
    // by CrostiniManager, so here we just (re)set the kernel version pref to
    // the empty string in case the response is empty.
    std::string kernel_version;
    if (maybe_kernel_version.has_value()) {
      kernel_version = maybe_kernel_version.value();
    }
    WriteTerminaVmKernelVersionToPrefsForReporting(profile_->GetPrefs(),
                                                   kernel_version);
  }

  void CreateLxdContainerFinished(CrostiniResult result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    for (auto& observer : observer_list_) {
      observer.OnContainerCreated(result);
    }
    if (ReturnEarlyIfAborted()) {
      return;
    }
    if (result != CrostiniResult::SUCCESS) {
      LOG(ERROR) << "Failed to Create Lxd Container.";
      FinishRestart(result);
      return;
    }
    StartStage(mojom::InstallerState::kSetupContainer);
    crostini_manager_->SetUpLxdContainerUser(
        vm_name_, container_name_,
        options_.container_username.value_or(
            DefaultContainerUserNameForProfile(profile_)),
        base::BindOnce(&CrostiniRestarter::SetUpLxdContainerUserFinished,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void SetUpLxdContainerUserFinished(bool success) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    for (auto& observer : observer_list_) {
      observer.OnContainerSetup(success);
    }
    if (ReturnEarlyIfAborted()) {
      return;
    }
    if (!success) {
      FinishRestart(CrostiniResult::CONTAINER_SETUP_FAILED);
      return;
    }

    StartStage(mojom::InstallerState::kStartContainer);
    crostini_manager_->StartLxdContainer(
        vm_name_, container_name_,
        base::BindOnce(&CrostiniRestarter::StartLxdContainerFinished,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void StartLxdContainerFinished(CrostiniResult result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    CloseCrostiniUpdateFilesystemView();
    for (auto& observer : observer_list_) {
      observer.OnContainerStarted(result);
    }
    if (ReturnEarlyIfAborted()) {
      return;
    }
    if (result != CrostiniResult::SUCCESS) {
      LOG(ERROR) << "Failed to Start Lxd Container.";
      FinishRestart(result);
      return;
    }
    // If default termina/penguin, then do device sharing, sshfs mount and
    // reshare folders, else we are finished.
    auto vm_info = crostini_manager_->GetVmInfo(vm_name_);
    if (vm_info && !vm_info->usb_devices_shared &&
        vm_name_ == kCrostiniDefaultVmName &&
        chromeos::CrosUsbDetector::Get()) {
      // Connect shared devices to the vm.
      chromeos::CrosUsbDetector::Get()->ConnectSharedDevicesOnVmStartup(
          vm_name_);
      vm_info->usb_devices_shared = true;
    }
    // If arc sideloading is enabled, configure the container for that.
    crostini_manager_->ConfigureForArcSideload();
    auto info = crostini_manager_->GetContainerInfo(vm_name_, container_name_);
    if (vm_name_ == kCrostiniDefaultVmName &&
        container_name_ == kCrostiniDefaultContainerName && info &&
        !info->sshfs_mounted) {
      StartStage(mojom::InstallerState::kFetchSshKeys);
      crostini_manager_->GetContainerSshKeys(
          vm_name_, container_name_,
          base::BindOnce(&CrostiniRestarter::GetContainerSshKeysFinished,
                         weak_ptr_factory_.GetWeakPtr(), info->username));
    } else {
      FinishRestart(result);
    }
  }

  void GetContainerSshKeysFinished(const std::string& container_username,
                                   bool success,
                                   const std::string& container_public_key,
                                   const std::string& host_private_key,
                                   const std::string& hostname) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    for (auto& observer : observer_list_) {
      observer.OnSshKeysFetched(success);
    }
    if (ReturnEarlyIfAborted()) {
      return;
    }
    if (!success) {
      FinishRestart(CrostiniResult::GET_CONTAINER_SSH_KEYS_FAILED);
      return;
    }

    // Add DiskMountManager::OnMountEvent observer.
    auto* dmgr = chromeos::disks::DiskMountManager::GetInstance();
    dmgr->AddObserver(this);

    // Call to sshfs to mount.
    source_path_ = base::StringPrintf(
        "sshfs://%s@%s:", container_username.c_str(), hostname.c_str());
    StartStage(mojom::InstallerState::kMountContainer);
    dmgr->MountPath(source_path_, "",
                    file_manager::util::GetCrostiniMountPointName(profile_),
                    file_manager::util::GetCrostiniMountOptions(
                        hostname, host_private_key, container_public_key),
                    chromeos::MOUNT_TYPE_NETWORK_STORAGE,
                    chromeos::MOUNT_ACCESS_MODE_READ_WRITE);
  }

  // chromeos::disks::DiskMountManager::Observer
  void OnMountEvent(chromeos::disks::DiskMountManager::MountEvent event,
                    chromeos::MountError error_code,
                    const chromeos::disks::DiskMountManager::MountPointInfo&
                        mount_info) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Ignore any other mount/unmount events.
    if (event != chromeos::disks::DiskMountManager::MountEvent::MOUNTING ||
        mount_info.source_path != source_path_) {
      return;
    }
    bool success = error_code == chromeos::MountError::MOUNT_ERROR_NONE;
    for (auto& observer : observer_list_) {
      observer.OnContainerMounted(success);
    }
    // Remove DiskMountManager::OnMountEvent observer.
    chromeos::disks::DiskMountManager::GetInstance()->RemoveObserver(this);

    if (!success) {
      LOG(ERROR) << "Error mounting crostini container: error_code="
                 << error_code << ", source_path=" << mount_info.source_path
                 << ", mount_path=" << mount_info.mount_path
                 << ", mount_type=" << mount_info.mount_type
                 << ", mount_condition=" << mount_info.mount_condition;
      if (ReturnEarlyIfAborted()) {
        return;
      }
      FinishRestart(CrostiniResult::SSHFS_MOUNT_ERROR);
      return;
    }

    crostini_manager_->SetContainerSshfsMounted(vm_name_, container_name_,
                                                true);

    // Register filesystem and add volume to VolumeManager.
    base::FilePath mount_path = base::FilePath(mount_info.mount_path);
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        file_manager::util::GetCrostiniMountPointName(profile_),
        storage::kFileSystemTypeNativeLocal, storage::FileSystemMountOption(),
        mount_path);

    // VolumeManager is null in unittest.
    if (auto* vmgr = file_manager::VolumeManager::Get(profile_))
      vmgr->AddSshfsCrostiniVolume(mount_path);

    // Abort not checked until exiting this function.  On abort, do not
    // continue, but still remove observer and add volume as per above.
    if (ReturnEarlyIfAborted()) {
      return;
    }

    FinishRestart(CrostiniResult::SUCCESS);
  }

  Profile* profile_;
  // This isn't accessed after the CrostiniManager is destroyed and we need a
  // reference to it during the CrostiniRestarter destructor.
  CrostiniManager* crostini_manager_;

  std::string vm_name_;
  base::FilePath disk_path_;
  std::string container_name_;
  RestartOptions options_;
  std::string source_path_;
  bool is_initial_install_ = false;
  CrostiniManager::CrostiniResultCallback completed_callback_;
  std::vector<base::OnceClosure> abort_callbacks_;
  base::ObserverList<CrostiniManager::RestartObserver>::Unchecked
      observer_list_;
  CrostiniManager::RestartId restart_id_;
  bool is_aborted_ = false;
  bool is_running_ = false;

  static CrostiniManager::RestartId next_restart_id_;

  base::WeakPtrFactory<CrostiniRestarter> weak_ptr_factory_{this};
};

CrostiniManager::RestartId
    CrostiniManager::CrostiniRestarter::next_restart_id_ = 0;
bool CrostiniManager::is_cros_termina_registered_ = false;
// Unit tests need this initialized to true. In Browser tests and real life,
// it is updated via MaybeUpdateCrostini.
bool CrostiniManager::is_dev_kvm_present_ = true;

void CrostiniManager::UpdateVmState(std::string vm_name, VmState vm_state) {
  auto vm_info = running_vms_.find(std::move(vm_name));
  if (vm_info != running_vms_.end()) {
    vm_info->second.state = vm_state;
    return;
  }
  // This can happen normally when StopVm is called right after start up.
  LOG(WARNING) << "Attempted to set state for unknown vm: " << vm_name;
}

bool CrostiniManager::IsVmRunning(std::string vm_name) {
  auto vm_info = running_vms_.find(std::move(vm_name));
  if (vm_info != running_vms_.end()) {
    return vm_info->second.state == VmState::STARTED;
  }
  return false;
}

base::Optional<VmInfo> CrostiniManager::GetVmInfo(std::string vm_name) {
  auto it = running_vms_.find(std::move(vm_name));
  if (it != running_vms_.end())
    return it->second;
  return base::nullopt;
}

void CrostiniManager::AddRunningVmForTesting(std::string vm_name) {
  running_vms_[std::move(vm_name)] = VmInfo{VmState::STARTED};
}

void CrostiniManager::AddStoppingVmForTesting(std::string vm_name) {
  running_vms_[std::move(vm_name)] = VmInfo{VmState::STOPPING};
}

LinuxPackageInfo::LinuxPackageInfo() = default;
LinuxPackageInfo::LinuxPackageInfo(LinuxPackageInfo&&) = default;
LinuxPackageInfo::LinuxPackageInfo(const LinuxPackageInfo&) = default;
LinuxPackageInfo& LinuxPackageInfo::operator=(LinuxPackageInfo&&) = default;
LinuxPackageInfo& LinuxPackageInfo::operator=(const LinuxPackageInfo&) =
    default;
LinuxPackageInfo::~LinuxPackageInfo() = default;

ContainerInfo::ContainerInfo(std::string container_name,
                             std::string container_username,
                             std::string container_homedir,
                             std::string ipv4_address)
    : name(std::move(container_name)),
      username(std::move(container_username)),
      homedir(std::move(container_homedir)),
      ipv4_address(std::move(ipv4_address)) {}
ContainerInfo::~ContainerInfo() = default;
ContainerInfo::ContainerInfo(ContainerInfo&&) = default;
ContainerInfo::ContainerInfo(const ContainerInfo&) = default;
ContainerInfo& ContainerInfo::operator=(ContainerInfo&&) = default;
ContainerInfo& ContainerInfo::operator=(const ContainerInfo&) = default;

void CrostiniManager::SetContainerSshfsMounted(std::string vm_name,
                                               std::string container_name,
                                               bool is_mounted) {
  auto range = running_containers_.equal_range(std::move(vm_name));
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second.name == container_name) {
      it->second.sshfs_mounted = is_mounted;
    }
  }
}

namespace {

ContainerOsVersion VersionFromOsRelease(
    const vm_tools::cicerone::OsRelease& os_release) {
  if (os_release.id() == "debian") {
    if (os_release.version_id() == "9") {
      return ContainerOsVersion::kDebianStretch;
    } else if (os_release.version_id() == "10") {
      return ContainerOsVersion::kDebianBuster;
    } else {
      return ContainerOsVersion::kDebianOther;
    }
  }
  return ContainerOsVersion::kOtherOs;
}

bool IsUpgradableContainerVersion(ContainerOsVersion version) {
  return version == ContainerOsVersion::kDebianStretch;
}

}  // namespace

void CrostiniManager::SetContainerOsRelease(
    std::string vm_name,
    std::string container_name,
    const vm_tools::cicerone::OsRelease& os_release) {
  ContainerId container_id(vm_name, container_name);
  ContainerOsVersion version = VersionFromOsRelease(os_release);
  // Store the os release version in prefs. We can use this value to decide if
  // an upgrade can be offered.
  UpdateContainerPref(profile_, container_id, prefs::kContainerOsVersionKey,
                      base::Value(static_cast<int>(version)));

  base::Optional<ContainerOsVersion> old_version;
  auto it = container_os_releases_.find(container_id);
  if (it != container_os_releases_.end()) {
    old_version = VersionFromOsRelease(it->second);
  }

  VLOG(1) << container_id;
  VLOG(1) << "os_release.pretty_name " << os_release.pretty_name();
  VLOG(1) << "os_release.name " << os_release.name();
  VLOG(1) << "os_release.version " << os_release.version();
  VLOG(1) << "os_release.version_id " << os_release.version_id();
  VLOG(1) << "os_release.id " << os_release.id();
  container_os_releases_[container_id] = os_release;
  if (!old_version || *old_version != version) {
    for (auto& observer : crostini_container_properties_observers_) {
      observer.OnContainerOsReleaseChanged(
          container_id, IsUpgradableContainerVersion(version));
    }
  }
  base::UmaHistogramEnumeration("Crostini.ContainerOsVersion", version);
}

void CrostiniManager::ConfigureForArcSideload() {
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::SessionManagerClient::Get();
  if (!base::FeatureList::IsEnabled(features::kCrostiniArcSideload) ||
      !session_manager_client)
    return;
  session_manager_client->QueryAdbSideload(base::BindOnce(
      // We use a lambda to keep the arc sideloading implementation local, and
      // avoid header pollution. This means we have to manually check the weak
      // pointer is alive.
      [](base::WeakPtr<CrostiniManager> manager,
         chromeos::SessionManagerClient::AdbSideloadResponseCode response_code,
         bool is_allowed) {
        if (!manager || !is_allowed ||
            response_code != chromeos::SessionManagerClient::
                                 AdbSideloadResponseCode::SUCCESS) {
          return;
        }
        vm_tools::cicerone::ConfigureForArcSideloadRequest request;
        request.set_owner_id(manager->owner_id_);
        request.set_vm_name(std::move(kCrostiniDefaultVmName));
        request.set_container_name(std::move(kCrostiniDefaultContainerName));
        GetCiceroneClient()->ConfigureForArcSideload(
            request,
            base::BindOnce(
                [](base::Optional<
                    vm_tools::cicerone::ConfigureForArcSideloadResponse>
                       response) {
                  if (!response) {
                    LOG(ERROR) << "Failed to configure for arc sideloading: no "
                                  "response from vm";
                    return;
                  }
                  if (response->status() ==
                      vm_tools::cicerone::ConfigureForArcSideloadResponse::
                          SUCCEEDED) {
                    return;
                  }
                  LOG(ERROR) << "Failed to configure for arc sideloading: "
                             << response->failure_reason();
                }));
      },
      weak_ptr_factory_.GetWeakPtr()));
}

const vm_tools::cicerone::OsRelease* CrostiniManager::GetContainerOsRelease(
    const ContainerId& container_id) const {
  auto it = container_os_releases_.find(container_id);
  if (it != container_os_releases_.end()) {
    return &it->second;
  }
  return nullptr;
}

bool CrostiniManager::IsContainerUpgradeable(
    const ContainerId& container_id) const {
  ContainerOsVersion version = ContainerOsVersion::kUnknown;
  const auto* os_release = GetContainerOsRelease(container_id);
  if (os_release) {
    version = VersionFromOsRelease(*os_release);
  } else {
    // Check prefs instead.
    const base::Value* value = GetContainerPrefValue(
        profile_, container_id, prefs::kContainerOsVersionKey);
    if (value) {
      version = static_cast<ContainerOsVersion>(value->GetInt());
    }
  }
  return IsUpgradableContainerVersion(version);
}

bool CrostiniManager::ShouldPromptContainerUpgrade(
    const ContainerId& container_id) const {
  if (!CrostiniFeatures::Get()->IsContainerUpgradeUIAllowed(profile_)) {
    return false;
  }
  if (container_upgrade_prompt_shown_.count(container_id) != 0) {
    // Already shown the upgrade dialog.
    return false;
  }
  if (container_id !=
      ContainerId(kCrostiniDefaultVmName, kCrostiniDefaultContainerName)) {
    return false;
  }
  bool upgradable = IsContainerUpgradeable(container_id);
  return upgradable;
}

void CrostiniManager::UpgradePromptShown(const ContainerId& container_id) {
  container_upgrade_prompt_shown_.insert(container_id);
}

bool CrostiniManager::IsUncleanStartup() const {
  return is_unclean_startup_;
}

void CrostiniManager::SetUncleanStartupForTesting(bool is_unclean_startup) {
  is_unclean_startup_ = is_unclean_startup;
}

base::Optional<ContainerInfo> CrostiniManager::GetContainerInfo(
    std::string vm_name,
    std::string container_name) {
  if (!IsVmRunning(vm_name)) {
    return base::nullopt;
  }
  auto range = running_containers_.equal_range(std::move(vm_name));
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second.name == container_name) {
      return it->second;
    }
  }
  return base::nullopt;
}

void CrostiniManager::AddRunningContainerForTesting(std::string vm_name,
                                                    ContainerInfo info) {
  running_containers_.emplace(std::move(vm_name), info);
}

void CrostiniManager::UpdateLaunchMetricsForEnterpriseReporting() {
  PrefService* const profile_prefs = profile_->GetPrefs();
  const component_updater::ComponentUpdateService* const update_service =
      g_browser_process->component_updater();
  const base::Clock* const clock = base::DefaultClock::GetInstance();
  WriteMetricsForReportingToPrefsIfEnabled(profile_prefs, update_service,
                                           clock);
}

CrostiniManager* CrostiniManager::GetForProfile(Profile* profile) {
  return CrostiniManagerFactory::GetForProfile(profile);
}

CrostiniManager::CrostiniManager(Profile* profile)
    : profile_(profile), owner_id_(CryptohomeIdForProfile(profile)) {
  DCHECK(!profile_->IsOffTheRecord());
  GetCiceroneClient()->AddObserver(this);
  GetConciergeClient()->AddVmObserver(this);
  GetConciergeClient()->AddContainerObserver(this);
  GetAnomalyDetectorClient()->AddObserver(this);
  if (chromeos::PowerManagerClient::Get()) {
    chromeos::PowerManagerClient::Get()->AddObserver(this);
  }
  CrostiniThrottle::GetForBrowserContext(profile_);
}

CrostiniManager::~CrostiniManager() {
  RemoveDBusObservers();
}

base::WeakPtr<CrostiniManager> CrostiniManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void CrostiniManager::RemoveDBusObservers() {
  if (dbus_observers_removed_) {
    return;
  }
  dbus_observers_removed_ = true;
  GetCiceroneClient()->RemoveObserver(this);
  GetConciergeClient()->RemoveContainerObserver(this);
  GetAnomalyDetectorClient()->RemoveObserver(this);
  if (chromeos::PowerManagerClient::Get()) {
    chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  }
}

// static
bool CrostiniManager::IsCrosTerminaInstalled() {
  return is_cros_termina_registered_;
}

// static
bool CrostiniManager::IsDevKvmPresent() {
  return is_dev_kvm_present_;
}

void CrostiniManager::MaybeUpdateCrostini() {
  scoped_refptr<component_updater::CrOSComponentManager> component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  if (!component_manager) {
    // |component_manager| may be nullptr in unit tests.
    return;
  }
  // This is a new user session, perhaps using an old CrostiniManager.
  container_upgrade_prompt_shown_.clear();
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          CrostiniManager::CheckPathsAndComponents,
          g_browser_process->platform_part()->cros_component_manager()),
      base::BindOnce(&CrostiniManager::MaybeUpdateCrostiniAfterChecks,
                     weak_ptr_factory_.GetWeakPtr()));
  // Probe Concierge - if it's still running after an unclean shutdown, a
  // success response will be received.
  if (profile_->GetLastSessionExitType() == Profile::EXIT_CRASHED) {
    vm_tools::concierge::GetVmInfoRequest concierge_request;
    concierge_request.set_owner_id(owner_id_);
    concierge_request.set_name(kCrostiniDefaultVmName);
    GetConciergeClient()->GetVmInfo(
        std::move(concierge_request),
        base::BindOnce(
            [](base::WeakPtr<CrostiniManager> weak_this,
               base::Optional<vm_tools::concierge::GetVmInfoResponse> reply) {
              if (weak_this) {
                VLOG(1) << "Exit type: "
                        << static_cast<int>(Profile::EXIT_CRASHED);
                VLOG(1) << "GetVmInfo result: "
                        << (reply.has_value() && reply->success());
                weak_this->is_unclean_startup_ =
                    reply.has_value() && reply->success();
                if (weak_this->is_unclean_startup_) {
                  weak_this->RemoveUncleanSshfsMounts();
                }
              }
            },
            weak_ptr_factory_.GetWeakPtr()));
  }
}

// static
void CrostiniManager::CheckPathsAndComponents(
    scoped_refptr<component_updater::CrOSComponentManager> component_manager) {
  DCHECK(component_manager);
  is_dev_kvm_present_ = base::PathExists(base::FilePath("/dev/kvm"));
  is_cros_termina_registered_ = component_manager->IsRegisteredMayBlock(
      imageloader::kTerminaComponentName);
}

void CrostiniManager::MaybeUpdateCrostiniAfterChecks() {
  if (!is_dev_kvm_present_) {
    return;
  }
  if (!is_cros_termina_registered_) {
    return;
  }
  if (!CrostiniFeatures::Get()->IsAllowed(profile_)) {
    return;
  }
  termina_update_check_needed_ = true;
  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    // Can't do a component Load with kForce when offline.
    VLOG(1) << "Not online, so can't check now for cros-termina upgrade.";
    return;
  }
  InstallTerminaComponent(base::DoNothing());
}

using UpdatePolicy = component_updater::CrOSComponentManager::UpdatePolicy;

void CrostiniManager::InstallTerminaComponent(CrostiniResultCallback callback) {
  scoped_refptr<component_updater::CrOSComponentManager> component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  if (!component_manager) {
    // Running in a unit test. We still PostTask to prevent races.
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&CrostiniManager::OnInstallTerminaComponent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       true, component_manager_load_error_for_testing_,
                       base::FilePath()));
    return;
  }

  DCHECK(component_manager);

  bool major_update_required =
      is_cros_termina_registered_ &&
      component_manager->GetCompatiblePath(imageloader::kTerminaComponentName)
          .empty();
  bool is_offline = content::GetNetworkConnectionTracker()->IsOffline();

  if (major_update_required) {
    termina_update_check_needed_ = false;
    if (is_offline) {
      LOG(ERROR) << "Need to load a major component update, but we're offline.";
      // TODO(nverne): Show a dialog/notification here for online upgrade
      // required.
      std::move(callback).Run(CrostiniResult::OFFLINE_WHEN_UPGRADE_REQUIRED);
      return;
    }
  }

  UpdatePolicy update_policy;
  if (termina_update_check_needed_ && !is_offline) {
    // Don't use kForce all the time because it generates traffic to
    // ComponentUpdaterService. Also, it's only appropriate for minor version
    // updates. Not major version incompatiblility.
    update_policy = UpdatePolicy::kForce;
  } else {
    update_policy = UpdatePolicy::kDontForce;
  }

  component_manager->Load(
      imageloader::kTerminaComponentName,
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      update_policy,
      base::BindOnce(&CrostiniManager::OnInstallTerminaComponent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     update_policy == UpdatePolicy::kForce));
}

void CrostiniManager::OnInstallTerminaComponent(
    CrostiniResultCallback callback,
    bool is_update_checked,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  bool is_successful =
      error == component_updater::CrOSComponentManager::Error::NONE;

  if (is_successful) {
    is_cros_termina_registered_ = true;
  } else {
    LOG(ERROR)
        << "Failed to install the cros-termina component with error code: "
        << static_cast<int>(error);
    if (is_cros_termina_registered_ && is_update_checked) {
      scoped_refptr<component_updater::CrOSComponentManager> component_manager =
          g_browser_process->platform_part()->cros_component_manager();
      if (component_manager) {
        // Try again, this time with no update checking. The reason we do this
        // is that we may still be offline even when is_offline above was
        // false. It's notoriously difficult to know when you're really
        // connected to the Internet, and it's also possible to be unable to
        // connect to a service like ComponentUpdaterService even when you are
        // connected to the rest of the Internet.
        UpdatePolicy update_policy = UpdatePolicy::kDontForce;

        LOG(ERROR) << "Retrying cros-termina component load, no update check";
        // Load the existing component on disk.
        component_manager->Load(
            imageloader::kTerminaComponentName,
            component_updater::CrOSComponentManager::MountPolicy::kMount,
            update_policy,
            base::BindOnce(&CrostiniManager::OnInstallTerminaComponent,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           false));
        return;
      }
    }
  }

  if (is_successful && is_update_checked) {
    VLOG(1) << "cros-termina update check successful.";
    termina_update_check_needed_ = false;
  }
  CrostiniResult result = CrostiniResult::SUCCESS;
  if (!is_successful) {
    if (error ==
        component_updater::CrOSComponentManager::Error::UPDATE_IN_PROGRESS) {
      // Something else triggered an update that we have to wait on. We don't
      // know what, or when they will be finished, so just retry every 5 seconds
      // until we get a different result.
      base::PostDelayedTask(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(&CrostiniManager::InstallTerminaComponent,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          base::TimeDelta::FromSeconds(5));
      return;
    } else {
      result = CrostiniResult::LOAD_COMPONENT_FAILED;
    }
  }

  std::move(callback).Run(result);
}

bool CrostiniManager::UninstallTerminaComponent() {
  bool success = true;
  scoped_refptr<component_updater::CrOSComponentManager> component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  if (component_manager) {
    success = component_manager->Unload(imageloader::kTerminaComponentName);
  }
  if (success) {
    is_cros_termina_registered_ = false;
  }
  return success;
}

void CrostiniManager::StartConcierge(BoolCallback callback) {
  VLOG(1) << "Starting Concierge service";
  chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->StartConcierge(
      base::BindOnce(&CrostiniManager::OnStartConcierge,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::OnStartConcierge(BoolCallback callback, bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to start Concierge service";
    std::move(callback).Run(success);
    return;
  }
  VLOG(1) << "Concierge service started";
  VLOG(1) << "Waiting for Cicerone to announce availability.";

  GetCiceroneClient()->WaitForServiceToBeAvailable(std::move(callback));
}

void CrostiniManager::StopConcierge(BoolCallback callback) {
  VLOG(1) << "Stopping Concierge service";
  chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->StopConcierge(
      base::BindOnce(&CrostiniManager::OnStopConcierge,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::OnStopConcierge(BoolCallback callback, bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to stop Concierge service";
  } else {
    VLOG(1) << "Concierge service stopped";
  }
  std::move(callback).Run(success);
}

void CrostiniManager::CreateDiskImage(
    const base::FilePath& disk_path,
    vm_tools::concierge::StorageLocation storage_location,
    int64_t disk_size_bytes,
    CreateDiskImageCallback callback) {
  std::string disk_path_string = disk_path.AsUTF8Unsafe();
  if (disk_path_string.empty()) {
    LOG(ERROR) << "Disk path cannot be empty";
    std::move(callback).Run(
        /*success=*/false,
        vm_tools::concierge::DiskImageStatus::DISK_STATUS_UNKNOWN,
        base::FilePath());
    return;
  }

  vm_tools::concierge::CreateDiskImageRequest request;
  request.set_cryptohome_id(CryptohomeIdForProfile(profile_));
  request.set_disk_path(std::move(disk_path_string));
  // The type of disk image to be created.
  request.set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);

  if (storage_location != vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT) {
    LOG(ERROR) << "'" << storage_location
               << "' is not a valid storage location";
    std::move(callback).Run(
        /*success=*/false,
        vm_tools::concierge::DiskImageStatus::DISK_STATUS_UNKNOWN,
        base::FilePath());
    return;
  }
  request.set_storage_location(storage_location);
  // The logical size of the new disk image, in bytes.
  request.set_disk_size(std::move(disk_size_bytes));

  GetConciergeClient()->CreateDiskImage(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnCreateDiskImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::DestroyDiskImage(const base::FilePath& disk_path,
                                       BoolCallback callback) {
  std::string disk_path_string = disk_path.AsUTF8Unsafe();
  if (disk_path_string.empty()) {
    LOG(ERROR) << "Disk path cannot be empty";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  vm_tools::concierge::DestroyDiskImageRequest request;
  request.set_cryptohome_id(CryptohomeIdForProfile(profile_));
  request.set_disk_path(std::move(disk_path_string));

  GetConciergeClient()->DestroyDiskImage(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnDestroyDiskImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::ListVmDisks(ListVmDisksCallback callback) {
  vm_tools::concierge::ListVmDisksRequest request;
  request.set_cryptohome_id(CryptohomeIdForProfile(profile_));
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);

  GetConciergeClient()->ListVmDisks(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnListVmDisks,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::StartTerminaVm(std::string name,
                                     const base::FilePath& disk_path,
                                     size_t num_cores_disabled,
                                     BoolCallback callback) {
  if (name.empty()) {
    LOG(ERROR) << "name is required";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::string disk_path_string = disk_path.AsUTF8Unsafe();
  if (disk_path_string.empty()) {
    LOG(ERROR) << "Disk path cannot be empty";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  if (!GetAnomalyDetectorClient()->IsGuestFileCorruptionSignalConnected()) {
    LOG(ERROR) << "GuestFileCorruptionSignal not connected, will not be "
                  "able to detect file system corruption.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // When Crostini is blocked because of powerwash request, do not start VM,
  // but show notification requesting powerwash.
  policy::PowerwashRequirementsChecker pw_checker(
      policy::PowerwashRequirementsChecker::Context::kCrostini, profile_);
  if (pw_checker.GetState() !=
      policy::PowerwashRequirementsChecker::State::kNotRequired) {
    pw_checker.ShowNotification();
    std::move(callback).Run(/*success=*/false);
    return;
  }

  for (auto& observer : vm_starting_observers_) {
    observer.OnVmStarting();
  }

  vm_tools::concierge::StartVmRequest request;
  request.set_name(std::move(name));
  request.set_start_termina(true);
  request.set_owner_id(owner_id_);
  if (base::FeatureList::IsEnabled(chromeos::features::kCrostiniGpuSupport))
    request.set_enable_gpu(true);
  bool is_mic_sharing_enabled =
      IsMicSharingEnabled(profile_, crostini_mic_sharing_enabled_);
  if (is_mic_sharing_enabled) {
    request.set_enable_audio_capture(true);
  }
  const int32_t cpus = base::SysInfo::NumberOfProcessors() - num_cores_disabled;
  DCHECK_LT(0, cpus);
  request.set_cpus(cpus);

  vm_tools::concierge::DiskImage* disk_image = request.add_disks();
  disk_image->set_path(std::move(disk_path_string));
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(true);
  disk_image->set_do_mount(false);

  GetConciergeClient()->StartTerminaVm(
      request, base::BindOnce(&CrostiniManager::OnStartTerminaVm,
                              weak_ptr_factory_.GetWeakPtr(), request.name(),
                              std::move(callback)));
}

void CrostiniManager::StopVm(std::string name,
                             CrostiniResultCallback callback) {
  if (name.empty()) {
    LOG(ERROR) << "name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }

  UpdateVmState(name, VmState::STOPPING);

  vm_tools::concierge::StopVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_name(name);

  GetConciergeClient()->StopVm(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnStopVm, weak_ptr_factory_.GetWeakPtr(),
                     std::move(name), std::move(callback)));
}

void CrostiniManager::GetTerminaVmKernelVersion(
    GetTerminaVmKernelVersionCallback callback) {
  vm_tools::concierge::GetVmEnterpriseReportingInfoRequest request;
  request.set_vm_name(kCrostiniDefaultVmName);
  request.set_owner_id(owner_id_);
  GetConciergeClient()->GetVmEnterpriseReportingInfo(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnGetTerminaVmKernelVersion,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::CreateLxdContainer(std::string vm_name,
                                         std::string container_name,
                                         CrostiniResultCallback callback) {
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (!GetCiceroneClient()->IsLxdContainerCreatedSignalConnected() ||
      !GetCiceroneClient()->IsLxdContainerDownloadingSignalConnected()) {
    LOG(ERROR)
        << "Async call to CreateLxdContainer can't complete when signals "
           "are not connected.";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  vm_tools::cicerone::CreateLxdContainerRequest request;
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_owner_id(owner_id_);
  std::string image_server_url;
  scoped_refptr<component_updater::CrOSComponentManager> component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  if (component_manager) {
    image_server_url =
        component_manager->GetCompatiblePath("cros-crostini-image-server-url")
            .value();
  }
  request.set_image_server(image_server_url.empty()
                               ? kCrostiniDefaultImageServerUrl
                               : image_server_url);
  if (base::FeatureList::IsEnabled(
          chromeos::features::kCrostiniUseBusterImage)) {
    request.set_image_alias(kCrostiniBusterImageAlias);
  } else {
    request.set_image_alias(kCrostiniStretchImageAlias);
  }
  GetCiceroneClient()->CreateLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnCreateLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), request.vm_name(),
                     request.container_name(), std::move(callback)));
}

void CrostiniManager::DeleteLxdContainer(std::string vm_name,
                                         std::string container_name,
                                         BoolCallback callback) {
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  if (!GetCiceroneClient()->IsLxdContainerDeletedSignalConnected()) {
    LOG(ERROR)
        << "Async call to DeleteLxdContainer can't complete when signals "
           "are not connected.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  vm_tools::cicerone::DeleteLxdContainerRequest request;
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_owner_id(owner_id_);
  GetCiceroneClient()->DeleteLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnDeleteLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), request.vm_name(),
                     request.container_name(), std::move(callback)));
}

void CrostiniManager::OnDeleteLxdContainer(
    std::string vm_name,
    std::string container_name,
    BoolCallback callback,
    base::Optional<vm_tools::cicerone::DeleteLxdContainerResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to delete lxd container in vm. Empty response.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (response->status() ==
      vm_tools::cicerone::DeleteLxdContainerResponse::DELETING) {
    ContainerId container_id(vm_name, container_name);
    VLOG(1) << "Awaiting LxdContainerDeletedSignal for " << container_id;
    delete_lxd_container_callbacks_.emplace(std::move(container_id),
                                            std::move(callback));

  } else if (response->status() ==
             vm_tools::cicerone::DeleteLxdContainerResponse::DOES_NOT_EXIST) {
    RemoveLxdContainerFromPrefs(profile_, vm_name, container_name);
    std::move(callback).Run(/*success=*/true);

  } else {
    LOG(ERROR) << "Failed to delete container: " << response->failure_reason();
    std::move(callback).Run(/*success=*/false);
  }
}

void CrostiniManager::StartLxdContainer(std::string vm_name,
                                        std::string container_name,
                                        CrostiniResultCallback callback) {
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (!GetCiceroneClient()->IsContainerStartedSignalConnected() ||
      !GetCiceroneClient()->IsContainerShutdownSignalConnected() ||
      !GetCiceroneClient()->IsLxdContainerStartingSignalConnected()) {
    LOG(ERROR) << "Async call to StartLxdContainer can't complete when signals "
                  "are not connected.";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  vm_tools::cicerone::StartLxdContainerRequest request;
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_owner_id(owner_id_);
  if (auto* integration_service =
          drive::DriveIntegrationServiceFactory::GetForProfile(profile_)) {
    request.set_drivefs_mount_path(
        integration_service->GetMountPointPath().value());
  }
  GetCiceroneClient()->StartLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnStartLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), request.vm_name(),
                     request.container_name(), std::move(callback)));
}

void CrostiniManager::SetUpLxdContainerUser(std::string vm_name,
                                            std::string container_name,
                                            std::string container_username,
                                            BoolCallback callback) {
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  if (container_username.empty()) {
    LOG(ERROR) << "container_username is required";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  vm_tools::cicerone::SetUpLxdContainerUserRequest request;
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_owner_id(owner_id_);
  request.set_container_username(std::move(container_username));
  GetCiceroneClient()->SetUpLxdContainerUser(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnSetUpLxdContainerUser,
                     weak_ptr_factory_.GetWeakPtr(), request.vm_name(),
                     request.container_name(), std::move(callback)));
}

void CrostiniManager::ExportLxdContainer(
    std::string vm_name,
    std::string container_name,
    base::FilePath export_path,
    ExportLxdContainerResultCallback callback) {
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR, 0, 0);
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR, 0, 0);
    return;
  }
  if (export_path.empty()) {
    LOG(ERROR) << "export_path is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR, 0, 0);
    return;
  }

  ContainerId key(vm_name, container_name);
  if (export_lxd_container_callbacks_.find(key) !=
      export_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "Export currently in progress for " << key;
    std::move(callback).Run(CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED, 0,
                            0);
    return;
  }
  export_lxd_container_callbacks_.emplace(key, std::move(callback));

  vm_tools::cicerone::ExportLxdContainerRequest request;
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_owner_id(owner_id_);
  request.set_export_path(export_path.value());
  GetCiceroneClient()->ExportLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnExportLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), request.vm_name(),
                     request.container_name()));
}

void CrostiniManager::ImportLxdContainer(std::string vm_name,
                                         std::string container_name,
                                         base::FilePath import_path,
                                         CrostiniResultCallback callback) {
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (import_path.empty()) {
    LOG(ERROR) << "import_path is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  ContainerId key(vm_name, container_name);
  if (import_lxd_container_callbacks_.find(key) !=
      import_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "Import currently in progress for " << key;
    std::move(callback).Run(CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED);
    return;
  }
  import_lxd_container_callbacks_.emplace(key, std::move(callback));

  vm_tools::cicerone::ImportLxdContainerRequest request;
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_owner_id(owner_id_);
  request.set_import_path(import_path.value());
  GetCiceroneClient()->ImportLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnImportLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), request.vm_name(),
                     request.container_name()));
}

void CrostiniManager::CancelExportLxdContainer(ContainerId key) {
  const auto& vm_name = key.vm_name;
  const auto& container_name = key.container_name;
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    return;
  }

  auto it = export_lxd_container_callbacks_.find(key);
  if (it == export_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "No export currently in progress for " << key;
    return;
  }

  vm_tools::cicerone::CancelExportLxdContainerRequest request;
  request.set_vm_name(vm_name);
  request.set_owner_id(owner_id_);
  request.set_in_progress_container_name(container_name);
  GetCiceroneClient()->CancelExportLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnCancelExportLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(key)));
}

void CrostiniManager::CancelImportLxdContainer(ContainerId key) {
  const auto& vm_name = key.vm_name;
  const auto& container_name = key.container_name;
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    return;
  }

  auto it = import_lxd_container_callbacks_.find(key);
  if (it == import_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "No import currently in progress for " << key;
    return;
  }

  vm_tools::cicerone::CancelImportLxdContainerRequest request;
  request.set_vm_name(vm_name);
  request.set_owner_id(owner_id_);
  request.set_in_progress_container_name(container_name);
  GetCiceroneClient()->CancelImportLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnCancelImportLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(key)));
}

namespace {
vm_tools::cicerone::UpgradeContainerRequest::Version ConvertVersion(
    ContainerVersion from) {
  switch (from) {
    case ContainerVersion::STRETCH:
      return vm_tools::cicerone::UpgradeContainerRequest::DEBIAN_STRETCH;
    case ContainerVersion::BUSTER:
      return vm_tools::cicerone::UpgradeContainerRequest::DEBIAN_BUSTER;
    case ContainerVersion::UNKNOWN:
    default:
      return vm_tools::cicerone::UpgradeContainerRequest::UNKNOWN;
  }
}

// Watches the Crostini restarter until the VM started phase, then aborts the
// sequence.
class AbortOnVmStartObserver : public CrostiniManager::RestartObserver {
 public:
  explicit AbortOnVmStartObserver(
      base::WeakPtr<CrostiniManager> crostini_manager)
      : crostini_manager_(crostini_manager) {}
  void OnVmStarted(bool success) override {
    if (crostini_manager_) {
      crostini_manager_->AbortRestartCrostini(restart_id(), base::DoNothing());
    }
  }

 private:
  base::WeakPtr<CrostiniManager> crostini_manager_;
};

}  // namespace

void CrostiniManager::UpgradeContainer(const ContainerId& key,
                                       ContainerVersion source_version,
                                       ContainerVersion target_version,
                                       CrostiniResultCallback callback) {
  const auto& vm_name = key.vm_name;
  const auto& container_name = key.container_name;
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (!GetCiceroneClient()->IsUpgradeContainerProgressSignalConnected()) {
    // Technically we could still start the upgrade, but we wouldn't be able
    // to detect when the upgrade completes, successfully or otherwise.
    LOG(ERROR) << "Attempted to upgrade container when progress signal not "
                  "connected.";
    std::move(callback).Run(CrostiniResult::UPGRADE_CONTAINER_FAILED);
    return;
  }
  vm_tools::cicerone::UpgradeContainerRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(vm_name);
  request.set_container_name(container_name);
  request.set_source_version(ConvertVersion(source_version));
  request.set_target_version(ConvertVersion(target_version));

  CrostiniResultCallback do_upgrade_container = base::BindOnce(
      [](base::WeakPtr<CrostiniManager> crostini_manager,
         vm_tools::cicerone::UpgradeContainerRequest request,
         CrostiniResultCallback final_callback, CrostiniResult result) {
        // When we fail to start the VM, we can't continue the upgrade.
        if (result != CrostiniResult::SUCCESS &&
            result != CrostiniResult::RESTART_ABORTED) {
          LOG(ERROR) << "Failed to restart the vm before attempting container "
                        "upgrade. Result code "
                     << static_cast<int>(result);
          std::move(final_callback)
              .Run(CrostiniResult::UPGRADE_CONTAINER_FAILED);
          return;
        }
        GetCiceroneClient()->UpgradeContainer(
            std::move(request),
            base::BindOnce(&CrostiniManager::OnUpgradeContainer,
                           crostini_manager, std::move(final_callback)));
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(request), std::move(callback));

  if (!IsVmRunning(vm_name)) {
    RestartCrostini(vm_name, container_name, std::move(do_upgrade_container));
  } else {
    std::move(do_upgrade_container).Run(CrostiniResult::SUCCESS);
  }
}

void CrostiniManager::EnsureVmRunning(const ContainerId& key,
                                      CrostiniResultCallback callback) {
  const auto& vm_name = key.vm_name;
  const auto& container_name = key.container_name;
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }

  CrostiniResultCallback inner_callback = base::BindOnce(
      [](CrostiniResultCallback final_callback, CrostiniResult result) {
        if (result == CrostiniResult::SUCCESS ||
            result == CrostiniResult::RESTART_ABORTED) {
          // RESTART_ABORTED is expected when we successfully abort after
          // launching the VM, turn it into a success since that's what we were
          // asked for.
          std::move(final_callback).Run(CrostiniResult::SUCCESS);
        } else {
          std::move(final_callback).Run(result);
        }
      },
      std::move(callback));

  if (!IsVmRunning(vm_name)) {
    RestartCrostini(vm_name, container_name, std::move(inner_callback),
                    new AbortOnVmStartObserver(weak_ptr_factory_.GetWeakPtr()));
  } else {
    std::move(inner_callback).Run(CrostiniResult::SUCCESS);
  }
}

void CrostiniManager::CancelUpgradeContainer(const ContainerId& key,
                                             CrostiniResultCallback callback) {
  const auto& vm_name = key.vm_name;
  const auto& container_name = key.container_name;
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  vm_tools::cicerone::CancelUpgradeContainerRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(vm_name);
  request.set_container_name(container_name);
  GetCiceroneClient()->CancelUpgradeContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnCancelUpgradeContainer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::LaunchContainerApplication(
    std::string vm_name,
    std::string container_name,
    std::string desktop_file_id,
    const std::vector<std::string>& files,
    bool display_scaled,
    BoolCallback callback) {
  vm_tools::cicerone::LaunchContainerApplicationRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_desktop_file_id(std::move(desktop_file_id));
  if (display_scaled) {
    request.set_display_scaling(
        vm_tools::cicerone::LaunchContainerApplicationRequest::SCALED);
  }
  std::copy(
      files.begin(), files.end(),
      google::protobuf::RepeatedFieldBackInserter(request.mutable_files()));

  GetCiceroneClient()->LaunchContainerApplication(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnLaunchContainerApplication,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::GetContainerAppIcons(
    std::string vm_name,
    std::string container_name,
    std::vector<std::string> desktop_file_ids,
    int icon_size,
    int scale,
    GetContainerAppIconsCallback callback) {
  vm_tools::cicerone::ContainerAppIconRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  google::protobuf::RepeatedPtrField<std::string> ids(
      std::make_move_iterator(desktop_file_ids.begin()),
      std::make_move_iterator(desktop_file_ids.end()));
  request.mutable_desktop_file_ids()->Swap(&ids);
  request.set_size(icon_size);
  request.set_scale(scale);

  GetCiceroneClient()->GetContainerAppIcons(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnGetContainerAppIcons,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::GetLinuxPackageInfo(
    Profile* profile,
    std::string vm_name,
    std::string container_name,
    std::string package_path,
    GetLinuxPackageInfoCallback callback) {
  vm_tools::cicerone::LinuxPackageInfoRequest request;
  request.set_owner_id(CryptohomeIdForProfile(profile));
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_file_path(std::move(package_path));

  GetCiceroneClient()->GetLinuxPackageInfo(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnGetLinuxPackageInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::InstallLinuxPackage(
    std::string vm_name,
    std::string container_name,
    std::string package_path,
    InstallLinuxPackageCallback callback) {
  if (!CrostiniFeatures::Get()->IsRootAccessAllowed(profile_)) {
    LOG(ERROR) << "Attempted to install package when root access to Crostini "
                  "VM not allowed.";
    std::move(callback).Run(CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
    return;
  }

  if (!GetCiceroneClient()->IsInstallLinuxPackageProgressSignalConnected()) {
    // Technically we could still start the install, but we wouldn't be able
    // to detect when the install completes, successfully or otherwise.
    LOG(ERROR)
        << "Attempted to install package when progress signal not connected.";
    std::move(callback).Run(CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
    return;
  }

  vm_tools::cicerone::InstallLinuxPackageRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_file_path(std::move(package_path));

  GetCiceroneClient()->InstallLinuxPackage(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnInstallLinuxPackage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::InstallLinuxPackageFromApt(
    const std::string& vm_name,
    const std::string& container_name,
    const std::string& package_id,
    InstallLinuxPackageCallback callback) {
  if (!GetCiceroneClient()->IsInstallLinuxPackageProgressSignalConnected()) {
    // Technically we could still start the install, but we wouldn't be able
    // to detect when the install completes, successfully or otherwise.
    LOG(ERROR)
        << "Attempted to install package when progress signal not connected.";
    std::move(callback).Run(CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
    return;
  }

  vm_tools::cicerone::InstallLinuxPackageRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(vm_name);
  request.set_container_name(container_name);
  request.set_package_id(package_id);

  GetCiceroneClient()->InstallLinuxPackage(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnInstallLinuxPackage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::UninstallPackageOwningFile(
    std::string vm_name,
    std::string container_name,
    std::string desktop_file_id,
    CrostiniResultCallback callback) {
  if (!GetCiceroneClient()->IsUninstallPackageProgressSignalConnected()) {
    // Technically we could still start the uninstall, but we wouldn't be able
    // to detect when the uninstall completes, successfully or otherwise.
    LOG(ERROR) << "Attempted to uninstall package when progress signal not "
                  "connected.";
    std::move(callback).Run(CrostiniResult::UNINSTALL_PACKAGE_FAILED);
    return;
  }

  vm_tools::cicerone::UninstallPackageOwningFileRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_desktop_file_id(std::move(desktop_file_id));

  GetCiceroneClient()->UninstallPackageOwningFile(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnUninstallPackageOwningFile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::GetContainerSshKeys(
    std::string vm_name,
    std::string container_name,
    GetContainerSshKeysCallback callback) {
  vm_tools::concierge::ContainerSshKeysRequest request;
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_cryptohome_id(CryptohomeIdForProfile(profile_));

  GetConciergeClient()->GetContainerSshKeys(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnGetContainerSshKeys,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool CrostiniManager::GetCrostiniDialogStatus(DialogType dialog_type) const {
  return open_crostini_dialogs_.count(dialog_type) == 1;
}

void CrostiniManager::SetCrostiniDialogStatus(DialogType dialog_type,
                                              bool open) {
  if (open) {
    open_crostini_dialogs_.insert(dialog_type);
  } else {
    open_crostini_dialogs_.erase(dialog_type);
  }
  for (auto& observer : crostini_dialog_status_observers_) {
    observer.OnCrostiniDialogStatusChanged(dialog_type, open);
  }
}

void CrostiniManager::AddCrostiniDialogStatusObserver(
    CrostiniDialogStatusObserver* observer) {
  crostini_dialog_status_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveCrostiniDialogStatusObserver(
    CrostiniDialogStatusObserver* observer) {
  crostini_dialog_status_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddCrostiniContainerPropertiesObserver(
    CrostiniContainerPropertiesObserver* observer) {
  crostini_container_properties_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveCrostiniContainerPropertiesObserver(
    CrostiniContainerPropertiesObserver* observer) {
  crostini_container_properties_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddContainerStartedObserver(
    ContainerStartedObserver* observer) {
  container_started_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveContainerStartedObserver(
    ContainerStartedObserver* observer) {
  container_started_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddContainerShutdownObserver(
    ContainerShutdownObserver* observer) {
  container_shutdown_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveContainerShutdownObserver(
    ContainerShutdownObserver* observer) {
  container_shutdown_observers_.RemoveObserver(observer);
}

void CrostiniManager::OnDBusShuttingDownForTesting() {
  RemoveDBusObservers();
}

void CrostiniManager::AttachUsbDevice(const std::string& vm_name,
                                      device::mojom::UsbDeviceInfoPtr device,
                                      base::ScopedFD fd,
                                      AttachUsbDeviceCallback callback) {
  vm_tools::concierge::AttachUsbDeviceRequest request;
  request.set_vm_name(vm_name);
  request.set_owner_id(CryptohomeIdForProfile(profile_));
  request.set_bus_number(device->bus_number);
  request.set_port_number(device->port_number);
  request.set_vendor_id(device->vendor_id);
  request.set_product_id(device->product_id);

  GetConciergeClient()->AttachUsbDevice(
      std::move(fd), std::move(request),
      base::BindOnce(&CrostiniManager::OnAttachUsbDevice,
                     weak_ptr_factory_.GetWeakPtr(), vm_name, std::move(device),
                     std::move(callback)));
}

void CrostiniManager::OnAttachUsbDevice(
    const std::string& vm_name,
    device::mojom::UsbDeviceInfoPtr device,
    AttachUsbDeviceCallback callback,
    base::Optional<vm_tools::concierge::AttachUsbDeviceResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to attach USB device, empty dbus response";
    std::move(callback).Run(/*success=*/false, chromeos::kInvalidUsbPortNumber);
    return;
  }

  if (!response->success()) {
    LOG(ERROR) << "Failed to attach USB device, " << response->reason();
    std::move(callback).Run(/*success=*/false, chromeos::kInvalidUsbPortNumber);
    return;
  }

  std::move(callback).Run(/*success=*/true, response->guest_port());
}

void CrostiniManager::DetachUsbDevice(const std::string& vm_name,
                                      device::mojom::UsbDeviceInfoPtr device,
                                      uint8_t guest_port,
                                      BoolCallback callback) {
  vm_tools::concierge::DetachUsbDeviceRequest request;
  request.set_vm_name(vm_name);
  request.set_owner_id(CryptohomeIdForProfile(profile_));
  request.set_guest_port(guest_port);

  GetConciergeClient()->DetachUsbDevice(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnDetachUsbDevice,
                     weak_ptr_factory_.GetWeakPtr(), vm_name, guest_port,
                     std::move(device), std::move(callback)));
}

void CrostiniManager::OnDetachUsbDevice(
    const std::string& vm_name,
    uint8_t guest_port,
    device::mojom::UsbDeviceInfoPtr device,
    BoolCallback callback,
    base::Optional<vm_tools::concierge::DetachUsbDeviceResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to detach USB device, empty dbus response";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (!response->success()) {
    LOG(ERROR) << "Failed to detach USB device, " << response->reason();
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::move(callback).Run(/*success=*/true);
}

CrostiniManager::RestartId CrostiniManager::RestartCrostini(
    std::string vm_name,
    std::string container_name,
    CrostiniResultCallback callback,
    RestartObserver* observer) {
  return RestartCrostiniWithOptions(std::move(vm_name),
                                    std::move(container_name), RestartOptions{},
                                    std::move(callback), observer);
}

CrostiniManager::RestartId CrostiniManager::RestartCrostiniWithOptions(
    std::string vm_name,
    std::string container_name,
    RestartOptions options,
    CrostiniResultCallback callback,
    RestartObserver* observer) {
  if (GetCrostiniDialogStatus(DialogType::INSTALLER)) {
    base::UmaHistogramBoolean("Crostini.Setup.Started", true);
  } else {
    base::UmaHistogramBoolean("Crostini.Restarter.Started", true);
  }
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Currently, |remove_crostini_callbacks_| is only used just before running
  // CrostiniRemover. If that changes, then we should check for a currently
  // running uninstaller in some other way.
  if (!remove_crostini_callbacks_.empty()) {
    LOG(ERROR)
        << "Tried to install crostini while crostini uninstaller is running";
    std::move(callback).Run(CrostiniResult::CROSTINI_UNINSTALLER_RUNNING);
    return kUninitializedRestartId;
  }

  auto restarter = std::make_unique<CrostiniRestarter>(
      profile_, this, std::move(vm_name), std::move(container_name),
      std::move(options), std::move(callback));
  auto restart_id = restarter->restart_id();
  if (observer)
    restarter->AddObserver(observer);
  auto key = ContainerId(restarter->vm_name(), restarter->container_name());
  restarters_by_container_.emplace(key, restart_id);
  restarters_by_id_[restart_id] = std::move(restarter);
  if (restarters_by_container_.count(key) > 1) {
    VLOG(1) << "Already restarting " << key;
  } else {
    // ::Restart needs to be called after the restarter is inserted into
    // restarters_by_id_ because some tests will make the restart process
    // complete before ::Restart returns.
    restarters_by_id_[restart_id]->Restart();
  }

  return restart_id;
}

void CrostiniManager::AbortRestartCrostini(
    CrostiniManager::RestartId restart_id,
    base::OnceClosure callback) {
  auto restarter_it = restarters_by_id_.find(restart_id);
  if (restarter_it == restarters_by_id_.end()) {
    // This can happen if a user cancels the install flow at the exact right
    // moment, for example.
    LOG(ERROR) << "Aborting a restarter that already finished";
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(callback));
    return;
  }
  restarter_it->second->Abort(base::BindOnce(
      &CrostiniManager::OnAbortRestartCrostini, weak_ptr_factory_.GetWeakPtr(),
      restart_id, std::move(callback)));
}

void CrostiniManager::OnAbortRestartCrostini(
    CrostiniManager::RestartId restart_id,
    base::OnceClosure callback) {
  auto restarter_it = restarters_by_id_.find(restart_id);
  if (restarter_it == restarters_by_id_.end()) {
    // This can happen if a user cancels the install flow at the exact right
    // moment, for example.
    LOG(ERROR) << "Aborting a restarter that already finished";
    std::move(callback).Run();
    return;
  }

  const ContainerId key(restarter_it->second->vm_name(),
                        restarter_it->second->container_name());
  auto range = restarters_by_container_.equal_range(key);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second == restart_id) {
      restarters_by_container_.erase(it);
      break;
    }
  }
  // This invalidates the iterator and potentially destroys the restarter,
  // so those shouldn't be accessed after this.
  restarters_by_id_.erase(restarter_it);

  // Kick off the "next" (in no order) pending Restart() if any.
  auto pending_it = restarters_by_container_.find(key);
  if (pending_it != restarters_by_container_.end()) {
    restarters_by_id_[pending_it->second]->Restart();
  }
  std::move(callback).Run();
}

bool CrostiniManager::IsRestartPending(RestartId restart_id) {
  auto it = restarters_by_id_.find(restart_id);
  return it != restarters_by_id_.end() && !it->second->is_aborted();
}

void CrostiniManager::AddShutdownContainerCallback(
    std::string vm_name,
    std::string container_name,
    base::OnceClosure shutdown_callback) {
  shutdown_container_callbacks_.emplace(ContainerId(vm_name, container_name),
                                        std::move(shutdown_callback));
}

void CrostiniManager::AddRemoveCrostiniCallback(
    RemoveCrostiniCallback remove_callback) {
  remove_crostini_callbacks_.emplace_back(std::move(remove_callback));
}

void CrostiniManager::AddLinuxPackageOperationProgressObserver(
    LinuxPackageOperationProgressObserver* observer) {
  linux_package_operation_progress_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveLinuxPackageOperationProgressObserver(
    LinuxPackageOperationProgressObserver* observer) {
  linux_package_operation_progress_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddPendingAppListUpdatesObserver(
    PendingAppListUpdatesObserver* observer) {
  pending_app_list_updates_observers_.AddObserver(observer);
}

void CrostiniManager::RemovePendingAppListUpdatesObserver(
    PendingAppListUpdatesObserver* observer) {
  pending_app_list_updates_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddExportContainerProgressObserver(
    ExportContainerProgressObserver* observer) {
  export_container_progress_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveExportContainerProgressObserver(
    ExportContainerProgressObserver* observer) {
  export_container_progress_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddImportContainerProgressObserver(
    ImportContainerProgressObserver* observer) {
  import_container_progress_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveImportContainerProgressObserver(
    ImportContainerProgressObserver* observer) {
  import_container_progress_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddUpgradeContainerProgressObserver(
    UpgradeContainerProgressObserver* observer) {
  upgrade_container_progress_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveUpgradeContainerProgressObserver(
    UpgradeContainerProgressObserver* observer) {
  upgrade_container_progress_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddVmShutdownObserver(VmShutdownObserver* observer) {
  vm_shutdown_observers_.AddObserver(observer);
}
void CrostiniManager::RemoveVmShutdownObserver(VmShutdownObserver* observer) {
  vm_shutdown_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddVmStartingObserver(
    chromeos::VmStartingObserver* observer) {
  vm_starting_observers_.AddObserver(observer);
}
void CrostiniManager::RemoveVmStartingObserver(
    chromeos::VmStartingObserver* observer) {
  vm_starting_observers_.RemoveObserver(observer);
}

void CrostiniManager::OnCreateDiskImage(
    CreateDiskImageCallback callback,
    base::Optional<vm_tools::concierge::CreateDiskImageResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to create disk image. Empty response.";
    std::move(callback).Run(/*success=*/false,
                            vm_tools::concierge::DISK_STATUS_UNKNOWN,
                            base::FilePath());
    return;
  }

  if (response->status() != vm_tools::concierge::DISK_STATUS_EXISTS &&
      response->status() != vm_tools::concierge::DISK_STATUS_CREATED) {
    LOG(ERROR) << "Failed to create disk image: " << response->failure_reason();
    std::move(callback).Run(/*success=*/false, response->status(),
                            base::FilePath());
    return;
  }

  std::move(callback).Run(/*success=*/true, response->status(),
                          base::FilePath(response->disk_path()));
}

void CrostiniManager::OnDestroyDiskImage(
    BoolCallback callback,
    base::Optional<vm_tools::concierge::DestroyDiskImageResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to destroy disk image. Empty response.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (response->status() != vm_tools::concierge::DISK_STATUS_DESTROYED &&
      response->status() != vm_tools::concierge::DISK_STATUS_DOES_NOT_EXIST) {
    LOG(ERROR) << "Failed to destroy disk image: "
               << response->failure_reason();
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::move(callback).Run(/*success=*/true);
}

void CrostiniManager::OnListVmDisks(
    ListVmDisksCallback callback,
    base::Optional<vm_tools::concierge::ListVmDisksResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to get list of VM disks. Empty response.";
    std::move(callback).Run(
        CrostiniResult::LIST_VM_DISKS_FAILED,
        profile_->GetPrefs()->GetInt64(prefs::kCrostiniLastDiskSize));
    return;
  }

  if (!response->success()) {
    LOG(ERROR) << "Failed to list VM disks: " << response->failure_reason();
    std::move(callback).Run(
        CrostiniResult::LIST_VM_DISKS_FAILED,
        profile_->GetPrefs()->GetInt64(prefs::kCrostiniLastDiskSize));
    return;
  }

  profile_->GetPrefs()->SetInt64(prefs::kCrostiniLastDiskSize,
                                 response->total_size());
  std::move(callback).Run(CrostiniResult::SUCCESS, response->total_size());
}

void CrostiniManager::OnStartTerminaVm(
    std::string vm_name,
    BoolCallback callback,
    base::Optional<vm_tools::concierge::StartVmResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to start termina vm. Empty response.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  switch (response->mount_result()) {
    case vm_tools::concierge::StartVmResponse::PARTIAL_DATA_LOSS:
      base::UmaHistogramEnumeration(kCrostiniCorruptionHistogram,
                                    CorruptionStates::MOUNT_ROLLED_BACK);
      break;
    case vm_tools::concierge::StartVmResponse::FAILURE:
      base::UmaHistogramEnumeration(kCrostiniCorruptionHistogram,
                                    CorruptionStates::MOUNT_FAILED);
      break;
    default:
      break;
  }

  // If the vm is already marked "running" run the callback.
  if (response->status() == vm_tools::concierge::VM_STATUS_RUNNING) {
    running_vms_[vm_name] =
        VmInfo{VmState::STARTED, std::move(response->vm_info())};
    std::move(callback).Run(/*success=*/true);
    return;
  }

  // Any pending callbacks must exist from a previously running VM, and should
  // be marked as failed.
  InvokeAndErasePendingCallbacks(
      &export_lxd_container_callbacks_, vm_name,
      CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STARTED, 0, 0);
  InvokeAndErasePendingCallbacks(
      &import_lxd_container_callbacks_, vm_name,
      CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STARTED);

  if (response->status() == vm_tools::concierge::VM_STATUS_FAILURE ||
      response->status() == vm_tools::concierge::VM_STATUS_UNKNOWN) {
    LOG(ERROR) << "Failed to start VM: " << response->failure_reason();
    // If we thought vms and containers were running before, they aren't now.
    running_vms_.erase(vm_name);
    running_containers_.erase(vm_name);
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // Otherwise, record the container start and run the callback after the VM
  // starts.
  DCHECK_EQ(response->status(), vm_tools::concierge::VM_STATUS_STARTING);
  VLOG(1) << "Awaiting TremplinStartedSignal for " << owner_id_ << ", "
          << vm_name;
  running_vms_[vm_name] =
      VmInfo{VmState::STARTING, std::move(response->vm_info())};
  // If we thought a container was running for this VM, we're wrong. This can
  // happen if the vm was formerly running, then stopped via crosh.
  running_containers_.erase(vm_name);

  tremplin_started_callbacks_.emplace(
      vm_name, base::BindOnce(&CrostiniManager::OnStartTremplin,
                              weak_ptr_factory_.GetWeakPtr(), vm_name,
                              std::move(callback)));
}

void CrostiniManager::OnStartTremplin(std::string vm_name,
                                      BoolCallback callback) {
  // Record the running vm.
  VLOG(1) << "Received TremplinStartedSignal, VM: " << owner_id_ << ", "
          << vm_name;
  UpdateVmState(vm_name, VmState::STARTED);

  // Share fonts directory with the VM but don't persist as a shared path.
  guest_os::GuestOsSharePath::GetForProfile(profile_)->SharePath(
      vm_name, base::FilePath(file_manager::util::kSystemFontsPath),
      /*persist=*/false, base::DoNothing());
  // Share folders from Downloads, etc with VM.
  guest_os::GuestOsSharePath::GetForProfile(profile_)->SharePersistedPaths(
      vm_name, base::DoNothing());

  // Run the original callback.
  std::move(callback).Run(/*success=*/true);
}

void CrostiniManager::OnStopVm(
    std::string vm_name,
    CrostiniResultCallback callback,
    base::Optional<vm_tools::concierge::StopVmResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to stop termina vm. Empty response.";
    std::move(callback).Run(CrostiniResult::VM_STOP_FAILED);
    return;
  }

  if (!response->success()) {
    LOG(ERROR) << "Failed to stop VM: " << response->failure_reason();
    // TODO(rjwright): Change the service so that "Requested VM does not
    // exist" is not an error. "Requested VM does not exist" means that there
    // is a disk image for the VM but it is not running, either because it has
    // not been started or it has already been stopped. There's no need for
    // this to be an error, and making it a success will save us having to
    // discriminate on failure_reason here.
    if (response->failure_reason() != "Requested VM does not exist") {
      std::move(callback).Run(CrostiniResult::VM_STOP_FAILED);
      return;
    }
  }

  std::move(callback).Run(CrostiniResult::SUCCESS);
}

void CrostiniManager::OnVmStoppedCleanup(const std::string& vm_name) {
  for (auto& observer : vm_shutdown_observers_) {
    observer.OnVmShutdown(vm_name);
  }

  // Remove from running_vms_, and other vm-keyed state.
  running_vms_.erase(vm_name);
  running_containers_.erase(vm_name);
  InvokeAndErasePendingCallbacks(
      &export_lxd_container_callbacks_, vm_name,
      CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED, 0, 0);
  InvokeAndErasePendingCallbacks(
      &import_lxd_container_callbacks_, vm_name,
      CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED);
  // After we shut down a VM, we are no longer in a state where we need to
  // prompt for user cleanup.
  is_unclean_startup_ = false;
}

void CrostiniManager::OnGetTerminaVmKernelVersion(
    GetTerminaVmKernelVersionCallback callback,
    base::Optional<vm_tools::concierge::GetVmEnterpriseReportingInfoResponse>
        response) {
  if (!response) {
    LOG(ERROR) << "No reply to GetVmEnterpriseReportingInfo";
    std::move(callback).Run(base::nullopt);
    return;
  }

  if (!response->success()) {
    LOG(ERROR) << "Error response for GetVmEnterpriseReportingInfo: "
               << response->failure_reason();
    std::move(callback).Run(base::nullopt);
    return;
  }

  std::move(callback).Run(response->vm_kernel_version());
}

void CrostiniManager::OnContainerStarted(
    const vm_tools::cicerone::ContainerStartedSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;

  running_containers_.emplace(
      signal.vm_name(),
      ContainerInfo(signal.container_name(), signal.container_username(),
                    signal.container_homedir(), signal.ipv4_address()));

  // Additional setup might be required in case of default Crostini container
  // such as installing Ansible in default container and applying
  // pre-determined configuration to the default container.
  if (signal.vm_name() == kCrostiniDefaultVmName &&
      signal.container_name() == kCrostiniDefaultContainerName &&
      ShouldConfigureDefaultContainer(profile_)) {
    AnsibleManagementService::GetForProfile(profile_)
        ->ConfigureDefaultContainer(
            base::BindOnce(&CrostiniManager::OnDefaultContainerConfigured,
                           weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  InvokeAndErasePendingContainerCallbacks(
      &start_container_callbacks_, signal.vm_name(), signal.container_name(),
      CrostiniResult::SUCCESS);

  if (signal.vm_name() == kCrostiniDefaultVmName) {
    AddShutdownContainerCallback(
        signal.vm_name(), signal.container_name(),
        base::Bind(&CrostiniManager::DeallocateForwardedPortsCallback,
                   weak_ptr_factory_.GetWeakPtr(), std::move(profile_),
                   ContainerId(signal.vm_name(), signal.container_name())));
    if (signal.container_name() == kCrostiniDefaultContainerName) {
      for (auto& observer : container_started_observers_) {
        observer.OnContainerStarted(
            ContainerId(signal.vm_name(), signal.container_name()));
      }
    }
  }
}

void CrostiniManager::OnDefaultContainerConfigured(bool success) {
  CrostiniResult result = CrostiniResult::SUCCESS;
  if (!success) {
    LOG(ERROR) << "Failed to configure default Crostini container";
    result = CrostiniResult::CONTAINER_CONFIGURATION_FAILED;
  }

  InvokeAndErasePendingContainerCallbacks(
      &start_container_callbacks_, kCrostiniDefaultVmName,
      kCrostiniDefaultContainerName, result);
}

void CrostiniManager::OnGuestFileCorruption(
    const anomaly_detector::GuestFileCorruptionSignal& signal) {
  base::UmaHistogramEnumeration(kCrostiniCorruptionHistogram,
                                CorruptionStates::OTHER_CORRUPTION);
}

void CrostiniManager::OnVmStarted(
    const vm_tools::concierge::VmStartedSignal& signal) {}

void CrostiniManager::OnVmStopped(
    const vm_tools::concierge::VmStoppedSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;
  OnVmStoppedCleanup(signal.name());
}

void CrostiniManager::OnContainerStartupFailed(
    const vm_tools::concierge::ContainerStartedSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;

  InvokeAndErasePendingContainerCallbacks(
      &start_container_callbacks_, signal.vm_name(), signal.container_name(),
      CrostiniResult::CONTAINER_START_FAILED);
}

void CrostiniManager::OnContainerShutdown(
    const vm_tools::cicerone::ContainerShutdownSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;
  if (signal.vm_name() == kCrostiniDefaultVmName &&
      signal.container_name() == kCrostiniDefaultContainerName) {
    for (auto& observer : container_shutdown_observers_) {
      observer.OnContainerShutdown(
          ContainerId(signal.vm_name(), signal.container_name()));
    }
  }
  // Find the callbacks to call, then erase them from the map.
  auto range_callbacks = shutdown_container_callbacks_.equal_range(
      ContainerId(signal.vm_name(), signal.container_name()));
  for (auto it = range_callbacks.first; it != range_callbacks.second; ++it) {
    std::move(it->second).Run();
  }
  shutdown_container_callbacks_.erase(range_callbacks.first,
                                      range_callbacks.second);

  // Remove from running containers multimap.
  auto range_containers = running_containers_.equal_range(signal.vm_name());
  for (auto it = range_containers.first; it != range_containers.second; ++it) {
    if (it->second.name == signal.container_name()) {
      running_containers_.erase(it);
      break;
    }
  }
}

void CrostiniManager::OnInstallLinuxPackageProgress(
    const vm_tools::cicerone::InstallLinuxPackageProgressSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;
  if (signal.progress_percent() < 0 || signal.progress_percent() > 100) {
    LOG(ERROR) << "Received install progress with invalid progress of "
               << signal.progress_percent() << "%.";
    return;
  }

  InstallLinuxPackageProgressStatus status;
  std::string error_message;
  switch (signal.status()) {
    case vm_tools::cicerone::InstallLinuxPackageProgressSignal::SUCCEEDED:
      status = InstallLinuxPackageProgressStatus::SUCCEEDED;
      break;
    case vm_tools::cicerone::InstallLinuxPackageProgressSignal::FAILED:
      LOG(ERROR) << "Install failed: " << signal.failure_details();
      status = InstallLinuxPackageProgressStatus::FAILED;
      error_message = signal.failure_details();
      break;
    case vm_tools::cicerone::InstallLinuxPackageProgressSignal::DOWNLOADING:
      status = InstallLinuxPackageProgressStatus::DOWNLOADING;
      break;
    case vm_tools::cicerone::InstallLinuxPackageProgressSignal::INSTALLING:
      status = InstallLinuxPackageProgressStatus::INSTALLING;
      break;
    default:
      NOTREACHED();
  }

  ContainerId container_id(signal.vm_name(), signal.container_name());
  for (auto& observer : linux_package_operation_progress_observers_) {
    observer.OnInstallLinuxPackageProgress(
        container_id, status, signal.progress_percent(), error_message);
  }
}

void CrostiniManager::OnUninstallPackageProgress(
    const vm_tools::cicerone::UninstallPackageProgressSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;

  if (signal.progress_percent() < 0 || signal.progress_percent() > 100) {
    LOG(ERROR) << "Received uninstall progress with invalid progress of "
               << signal.progress_percent() << "%.";
    return;
  }

  UninstallPackageProgressStatus status;
  switch (signal.status()) {
    case vm_tools::cicerone::UninstallPackageProgressSignal::SUCCEEDED:
      status = UninstallPackageProgressStatus::SUCCEEDED;
      break;
    case vm_tools::cicerone::UninstallPackageProgressSignal::FAILED:
      status = UninstallPackageProgressStatus::FAILED;
      LOG(ERROR) << "Uninstalled failed: " << signal.failure_details();
      break;
    case vm_tools::cicerone::UninstallPackageProgressSignal::UNINSTALLING:
      status = UninstallPackageProgressStatus::UNINSTALLING;
      break;
    default:
      NOTREACHED();
  }

  ContainerId container_id(signal.vm_name(), signal.container_name());
  for (auto& observer : linux_package_operation_progress_observers_) {
    observer.OnUninstallPackageProgress(container_id, status,
                                        signal.progress_percent());
  }
}

void CrostiniManager::OnApplyAnsiblePlaybookProgress(
    const vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;

  // TODO(okalitova): Add an observer.
  AnsibleManagementService::GetForProfile(profile_)
      ->OnApplyAnsiblePlaybookProgress(signal.status(),
                                       signal.failure_details());
}

void CrostiniManager::OnUpgradeContainerProgress(
    const vm_tools::cicerone::UpgradeContainerProgressSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;

  UpgradeContainerProgressStatus status;
  switch (signal.status()) {
    case vm_tools::cicerone::UpgradeContainerProgressSignal::SUCCEEDED:
      status = UpgradeContainerProgressStatus::SUCCEEDED;
      break;
    case vm_tools::cicerone::UpgradeContainerProgressSignal::UNKNOWN:
    case vm_tools::cicerone::UpgradeContainerProgressSignal::FAILED:
      status = UpgradeContainerProgressStatus::FAILED;
      LOG(ERROR) << "Upgrade failed: " << signal.failure_reason();
      break;
    case vm_tools::cicerone::UpgradeContainerProgressSignal::IN_PROGRESS:
      status = UpgradeContainerProgressStatus::UPGRADING;
      break;
    default:
      NOTREACHED();
  }

  std::vector<std::string> progress_messages;
  progress_messages.reserve(signal.progress_messages().size());
  for (const auto& msg : signal.progress_messages()) {
    if (!msg.empty()) {
      // Blank lines aren't sent to observers.
      progress_messages.push_back(msg);
    }
  }

  ContainerId container_id(signal.vm_name(), signal.container_name());
  for (auto& observer : upgrade_container_progress_observers_) {
    observer.OnUpgradeContainerProgress(container_id, status,
                                        progress_messages);
  }
}

void CrostiniManager::OnStartLxdProgress(
    const vm_tools::cicerone::StartLxdProgressSignal& signal) {
  // TODO(crbug/1064512): Implement.
}

void CrostiniManager::OnUninstallPackageOwningFile(
    CrostiniResultCallback callback,
    base::Optional<vm_tools::cicerone::UninstallPackageOwningFileResponse>
        response) {
  if (!response) {
    LOG(ERROR) << "Failed to uninstall Linux package. Empty response.";
    std::move(callback).Run(CrostiniResult::UNINSTALL_PACKAGE_FAILED);
    return;
  }

  if (response->status() ==
      vm_tools::cicerone::UninstallPackageOwningFileResponse::FAILED) {
    LOG(ERROR) << "Failed to uninstall Linux package: "
               << response->failure_reason();
    std::move(callback).Run(CrostiniResult::UNINSTALL_PACKAGE_FAILED);
    return;
  }

  if (response->status() ==
      vm_tools::cicerone::UninstallPackageOwningFileResponse::
          BLOCKING_OPERATION_IN_PROGRESS) {
    LOG(WARNING) << "Failed to uninstall Linux package, another operation is "
                    "already active.";
    std::move(callback).Run(CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE);
    return;
  }

  std::move(callback).Run(CrostiniResult::SUCCESS);
}

void CrostiniManager::OnCreateLxdContainer(
    std::string vm_name,
    std::string container_name,
    CrostiniResultCallback callback,
    base::Optional<vm_tools::cicerone::CreateLxdContainerResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to create lxd container in vm. Empty response.";
    std::move(callback).Run(CrostiniResult::CONTAINER_CREATE_FAILED);
    return;
  }

  if (response->status() ==
      vm_tools::cicerone::CreateLxdContainerResponse::CREATING) {
    ContainerId container_id(vm_name, container_name);
    VLOG(1) << "Awaiting LxdContainerCreatedSignal for " << owner_id_ << ", "
            << container_id;
    // The callback will be called when we receive the LxdContainerCreated
    // signal.
    create_lxd_container_callbacks_.emplace(std::move(container_id),
                                            std::move(callback));
    return;
  }
  if (response->status() !=
      vm_tools::cicerone::CreateLxdContainerResponse::EXISTS) {
    LOG(ERROR) << "Failed to start container: " << response->failure_reason();
    std::move(callback).Run(CrostiniResult::CONTAINER_CREATE_FAILED);
    return;
  }
  std::move(callback).Run(CrostiniResult::SUCCESS);
}

void CrostiniManager::OnStartLxdContainer(
    std::string vm_name,
    std::string container_name,
    CrostiniResultCallback callback,
    base::Optional<vm_tools::cicerone::StartLxdContainerResponse> response) {
  if (!response) {
    VLOG(1) << "Failed to start lxd container in vm. Empty response.";
    std::move(callback).Run(CrostiniResult::CONTAINER_START_FAILED);
    return;
  }

  switch (response->status()) {
    case vm_tools::cicerone::StartLxdContainerResponse::UNKNOWN:
    case vm_tools::cicerone::StartLxdContainerResponse::FAILED:
      LOG(ERROR) << "Failed to start container: " << response->failure_reason();
      std::move(callback).Run(CrostiniResult::CONTAINER_START_FAILED);
      break;

    case vm_tools::cicerone::StartLxdContainerResponse::STARTED:
    case vm_tools::cicerone::StartLxdContainerResponse::RUNNING:
      std::move(callback).Run(CrostiniResult::SUCCESS);
      break;

    case vm_tools::cicerone::StartLxdContainerResponse::REMAPPING:
      // Run the update container dialog to warn users of delays.
      // The callback will be called when we receive the LxdContainerStarting
      PrepareShowCrostiniUpdateFilesystemView(profile_,
                                              CrostiniUISurface::kAppList);
      // signal.
      // Then perform the same steps as for starting.
      FALLTHROUGH;
    case vm_tools::cicerone::StartLxdContainerResponse::STARTING: {
      ContainerId container_id(vm_name, container_name);
      VLOG(1) << "Awaiting LxdContainerStartingSignal for " << owner_id_ << ", "
              << container_id;
      // The callback will be called when we receive the LxdContainerStarting
      // signal and (if successful) the ContainerStarted signal from Garcon..
      start_container_callbacks_.emplace(std::move(container_id),
                                         std::move(callback));
      break;
    }
    default:
      NOTREACHED();
      break;
  }
  if (response->has_os_release()) {
    SetContainerOsRelease(vm_name, container_name, response->os_release());
  }
}

void CrostiniManager::OnSetUpLxdContainerUser(
    std::string vm_name,
    std::string container_name,
    BoolCallback callback,
    base::Optional<vm_tools::cicerone::SetUpLxdContainerUserResponse>
        response) {
  if (!response) {
    LOG(ERROR) << "Failed to set up lxd container user. Empty response.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (response->status() !=
          vm_tools::cicerone::SetUpLxdContainerUserResponse::SUCCESS &&
      response->status() !=
          vm_tools::cicerone::SetUpLxdContainerUserResponse::EXISTS) {
    LOG(ERROR) << "Failed to set up container user: "
               << response->failure_reason();
    std::move(callback).Run(/*success=*/false);
    return;
  }
  std::move(callback).Run(/*success=*/true);
}

void CrostiniManager::OnLxdContainerCreated(
    const vm_tools::cicerone::LxdContainerCreatedSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;
  CrostiniResult result;

  switch (signal.status()) {
    case vm_tools::cicerone::LxdContainerCreatedSignal::UNKNOWN:
      result = CrostiniResult::UNKNOWN_ERROR;
      break;
    case vm_tools::cicerone::LxdContainerCreatedSignal::CREATED:
      result = CrostiniResult::SUCCESS;
      AddNewLxdContainerToPrefs(profile_, signal.vm_name(),
                                signal.container_name());
      break;
    case vm_tools::cicerone::LxdContainerCreatedSignal::DOWNLOAD_TIMED_OUT:
      result = CrostiniResult::CONTAINER_DOWNLOAD_TIMED_OUT;
      break;
    case vm_tools::cicerone::LxdContainerCreatedSignal::CANCELLED:
      result = CrostiniResult::CONTAINER_CREATE_CANCELLED;
      break;
    case vm_tools::cicerone::LxdContainerCreatedSignal::FAILED:
      result = CrostiniResult::CONTAINER_CREATE_FAILED;
      break;
    default:
      result = CrostiniResult::UNKNOWN_ERROR;
      break;
  }

  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to create container. VM: " << signal.vm_name()
               << " container: " << signal.container_name()
               << " reason: " << signal.failure_reason();
  }

  InvokeAndErasePendingContainerCallbacks(&create_lxd_container_callbacks_,
                                          signal.vm_name(),
                                          signal.container_name(), result);
}

void CrostiniManager::OnLxdContainerDeleted(
    const vm_tools::cicerone::LxdContainerDeletedSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;

  ContainerId container_id(signal.vm_name(), signal.container_name());
  bool success =
      signal.status() == vm_tools::cicerone::LxdContainerDeletedSignal::DELETED;
  if (success) {
    RemoveLxdContainerFromPrefs(profile_, signal.vm_name(),
                                signal.container_name());
  } else {
    LOG(ERROR) << "Failed to delete container " << container_id << " : "
               << signal.failure_reason();
  }

  // Find the callbacks to call, then erase them from the map.
  auto range = delete_lxd_container_callbacks_.equal_range(container_id);
  for (auto it = range.first; it != range.second; ++it) {
    std::move(it->second).Run(success);
  }
  delete_lxd_container_callbacks_.erase(range.first, range.second);
}

void CrostiniManager::OnLxdContainerDownloading(
    const vm_tools::cicerone::LxdContainerDownloadingSignal& signal) {
  if (owner_id_ != signal.owner_id()) {
    return;
  }
  auto range = restarters_by_container_.equal_range(
      ContainerId(signal.vm_name(), signal.container_name()));
  for (auto it = range.first; it != range.second; ++it) {
    restarters_by_id_[it->second]->OnContainerDownloading(
        signal.download_progress());
  }
}

void CrostiniManager::OnTremplinStarted(
    const vm_tools::cicerone::TremplinStartedSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;
  // Find the callbacks to call, then erase them from the map.
  auto range = tremplin_started_callbacks_.equal_range(signal.vm_name());
  for (auto it = range.first; it != range.second; ++it) {
    std::move(it->second).Run();
  }
  tremplin_started_callbacks_.erase(range.first, range.second);
}

void CrostiniManager::OnLxdContainerStarting(
    const vm_tools::cicerone::LxdContainerStartingSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;
  CrostiniResult result;

  switch (signal.status()) {
    case vm_tools::cicerone::LxdContainerStartingSignal::UNKNOWN:
      result = CrostiniResult::UNKNOWN_ERROR;
      break;
    case vm_tools::cicerone::LxdContainerStartingSignal::CANCELLED:
      result = CrostiniResult::CONTAINER_START_CANCELLED;
      break;
    case vm_tools::cicerone::LxdContainerStartingSignal::STARTED:
      result = CrostiniResult::SUCCESS;
      break;
    case vm_tools::cicerone::LxdContainerStartingSignal::FAILED:
      result = CrostiniResult::CONTAINER_START_FAILED;
      break;
    default:
      result = CrostiniResult::UNKNOWN_ERROR;
      break;
  }

  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to start container. VM: " << signal.vm_name()
               << " container: " << signal.container_name()
               << " reason: " << signal.failure_reason();
  }

  if (result == CrostiniResult::SUCCESS &&
      !GetContainerInfo(signal.vm_name(), signal.container_name())) {
    VLOG(1) << "Awaiting ContainerStarted signal from Garcon";
    return;
  }
  if (signal.has_os_release()) {
    SetContainerOsRelease(signal.vm_name(), signal.container_name(),
                          signal.os_release());
  }

  InvokeAndErasePendingContainerCallbacks(&start_container_callbacks_,
                                          signal.vm_name(),
                                          signal.container_name(), result);
}

void CrostiniManager::OnLaunchContainerApplication(
    BoolCallback callback,
    base::Optional<vm_tools::cicerone::LaunchContainerApplicationResponse>
        response) {
  if (!response) {
    LOG(ERROR) << "Failed to launch application. Empty response.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (!response->success()) {
    LOG(ERROR) << "Failed to launch application: "
               << response->failure_reason();
    std::move(callback).Run(/*success=*/false);
    return;
  }
  std::move(callback).Run(/*success=*/true);
}

void CrostiniManager::OnGetContainerAppIcons(
    GetContainerAppIconsCallback callback,
    base::Optional<vm_tools::cicerone::ContainerAppIconResponse> response) {
  std::vector<Icon> icons;
  if (!response) {
    LOG(ERROR) << "Failed to get container application icons. Empty response.";
    std::move(callback).Run(/*success=*/false, icons);
    return;
  }

  for (auto& icon : *response->mutable_icons()) {
    icons.emplace_back(
        Icon{.desktop_file_id = std::move(*icon.mutable_desktop_file_id()),
             .content = std::move(*icon.mutable_icon())});
  }
  std::move(callback).Run(/*success=*/true, icons);
}

void CrostiniManager::OnGetLinuxPackageInfo(
    GetLinuxPackageInfoCallback callback,
    base::Optional<vm_tools::cicerone::LinuxPackageInfoResponse> response) {
  LinuxPackageInfo result;
  if (!response) {
    LOG(ERROR) << "Failed to get Linux package info. Empty response.";
    result.success = false;
    // The error message is currently only used in a console message. If we
    // want to display it to the user, we'd need to localize this.
    result.failure_reason = "D-Bus response was empty.";
    std::move(callback).Run(result);
    return;
  }

  if (!response->success()) {
    LOG(ERROR) << "Failed to get Linux package info: "
               << response->failure_reason();
    result.success = false;
    result.failure_reason = response->failure_reason();
    std::move(callback).Run(result);
    return;
  }

  // The |package_id| field is formatted like "name;version;arch;data". We're
  // currently only interested in name and version.
  std::vector<std::string> split = base::SplitString(
      response->package_id(), ";", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (split.size() < 2 || split[0].empty() || split[1].empty()) {
    LOG(ERROR) << "Linux package info contained invalid package id: \""
               << response->package_id() << '"';
    result.success = false;
    result.failure_reason = "Linux package info contained invalid package id.";
    std::move(callback).Run(result);
    return;
  }

  result.success = true;
  result.package_id = response->package_id();
  result.name = split[0];
  result.version = split[1];
  result.description = response->description();
  result.summary = response->summary();

  std::move(callback).Run(result);
}

void CrostiniManager::OnInstallLinuxPackage(
    InstallLinuxPackageCallback callback,
    base::Optional<vm_tools::cicerone::InstallLinuxPackageResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to install Linux package. Empty response.";
    std::move(callback).Run(CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
    return;
  }

  if (response->status() ==
      vm_tools::cicerone::InstallLinuxPackageResponse::FAILED) {
    LOG(ERROR) << "Failed to install Linux package: "
               << response->failure_reason();
    std::move(callback).Run(CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
    return;
  }

  if (response->status() ==
      vm_tools::cicerone::InstallLinuxPackageResponse::INSTALL_ALREADY_ACTIVE) {
    LOG(WARNING) << "Failed to install Linux package, install already active.";
    std::move(callback).Run(CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE);
    return;
  }

  std::move(callback).Run(CrostiniResult::SUCCESS);
}

void CrostiniManager::OnGetContainerSshKeys(
    GetContainerSshKeysCallback callback,
    base::Optional<vm_tools::concierge::ContainerSshKeysResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to get ssh keys. Empty response.";
    std::move(callback).Run(/*success=*/false, "", "", "");
    return;
  }
  std::move(callback).Run(/*success=*/true, response->container_public_key(),
                          response->host_private_key(), response->hostname());
}

void CrostiniManager::RemoveCrostini(std::string vm_name,
                                     RemoveCrostiniCallback callback) {
  AddRemoveCrostiniCallback(std::move(callback));

  auto crostini_remover = base::MakeRefCounted<CrostiniRemover>(
      profile_, std::move(vm_name),
      base::BindOnce(&CrostiniManager::OnRemoveCrostini,
                     weak_ptr_factory_.GetWeakPtr()));

  auto abort_callback = base::BarrierClosure(
      restarters_by_id_.size(),
      base::BindOnce(
          [](scoped_refptr<CrostiniRemover> remover) {
            base::PostTask(
                FROM_HERE, {content::BrowserThread::UI},
                base::BindOnce(&CrostiniRemover::RemoveCrostini, remover));
          },
          crostini_remover));

  for (const auto& restarter_it : restarters_by_id_) {
    AbortRestartCrostini(restarter_it.first, abort_callback);
  }
}

void CrostiniManager::OnRemoveCrostini(CrostiniResult result) {
  for (auto& callback : remove_crostini_callbacks_) {
    std::move(callback).Run(result);
  }
  remove_crostini_callbacks_.clear();
}

void CrostiniManager::FinishRestart(CrostiniRestarter* restarter,
                                    CrostiniResult result) {
  auto key = ContainerId(restarter->vm_name(), restarter->container_name());
  auto range = restarters_by_container_.equal_range(key);
  std::vector<std::unique_ptr<CrostiniRestarter>> pending_restarters;
  // Erase first, because restarter->RunCallback() may modify our maps.
  for (auto it = range.first; it != range.second; ++it) {
    CrostiniManager::RestartId restart_id = it->second;
    pending_restarters.emplace_back(std::move(restarters_by_id_[restart_id]));
    restarters_by_id_.erase(restart_id);
  }
  restarters_by_container_.erase(range.first, range.second);
  for (const auto& pending_restarter : pending_restarters) {
    pending_restarter->result_ = result;
    pending_restarter->RunCallback(result);
  }
}

void CrostiniManager::OnExportLxdContainer(
    std::string vm_name,
    std::string container_name,
    base::Optional<vm_tools::cicerone::ExportLxdContainerResponse> response) {
  ContainerId key(vm_name, container_name);
  auto it = export_lxd_container_callbacks_.find(key);
  if (it == export_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "No export callback for " << key;
    return;
  }

  if (!response) {
    LOG(ERROR) << "Failed to export lxd container. Empty response.";
    std::move(it->second)
        .Run(CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED, 0, 0);
    export_lxd_container_callbacks_.erase(it);
    return;
  }

  // If export has started, the callback will be invoked when the
  // ExportLxdContainerProgressSignal signal indicates that export is
  // complete, otherwise this is an error.
  if (response->status() !=
      vm_tools::cicerone::ExportLxdContainerResponse::EXPORTING) {
    LOG(ERROR) << "Failed to export container: status=" << response->status()
               << ", failure_reason=" << response->failure_reason();
    std::move(it->second)
        .Run(CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED, 0, 0);
    export_lxd_container_callbacks_.erase(it);
  }
}

void CrostiniManager::OnExportLxdContainerProgress(
    const vm_tools::cicerone::ExportLxdContainerProgressSignal& signal) {
  using ProgressSignal = vm_tools::cicerone::ExportLxdContainerProgressSignal;

  if (signal.owner_id() != owner_id_)
    return;

  const ContainerId container_id(signal.vm_name(), signal.container_name());

  CrostiniResult result;
  switch (signal.status()) {
    // TODO(juwa): Remove EXPORTING_[PACK|DOWNLOAD] once a new version of
    // tremplin has shipped.
    case ProgressSignal::EXPORTING_PACK:
    case ProgressSignal::EXPORTING_DOWNLOAD: {
      // If we are still exporting, call progress observers.
      const auto status = signal.status() == ProgressSignal::EXPORTING_PACK
                              ? ExportContainerProgressStatus::PACK
                              : ExportContainerProgressStatus::DOWNLOAD;
      for (auto& observer : export_container_progress_observers_) {
        observer.OnExportContainerProgress(container_id, status,
                                           signal.progress_percent(),
                                           signal.progress_speed());
      }
      return;
    }
    case ProgressSignal::EXPORTING_STREAMING: {
      const StreamingExportStatus status{
          .total_files = signal.total_input_files(),
          .total_bytes = signal.total_input_bytes(),
          .exported_files = signal.input_files_streamed(),
          .exported_bytes = signal.input_bytes_streamed()};
      for (auto& observer : export_container_progress_observers_) {
        observer.OnExportContainerProgress(container_id, status);
      }
      return;
    }
    case ProgressSignal::CANCELLED:
      result = CrostiniResult::CONTAINER_EXPORT_IMPORT_CANCELLED;
      break;
    case ProgressSignal::DONE:
      result = CrostiniResult::SUCCESS;
      break;
    default:
      result = CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED;
      LOG(ERROR) << "Failed during export container: " << signal.status()
                 << ", " << signal.failure_reason();
  }

  // Invoke original callback with either success or failure.
  auto it = export_lxd_container_callbacks_.find(container_id);
  if (it == export_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "No export callback for " << container_id;
    return;
  }
  std::move(it->second)
      .Run(result, signal.input_bytes_streamed(), signal.bytes_exported());
  export_lxd_container_callbacks_.erase(it);
}

void CrostiniManager::OnImportLxdContainer(
    std::string vm_name,
    std::string container_name,
    base::Optional<vm_tools::cicerone::ImportLxdContainerResponse> response) {
  ContainerId key(vm_name, container_name);
  auto it = import_lxd_container_callbacks_.find(key);
  if (it == import_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "No import callback for " << key;
    return;
  }

  if (!response) {
    LOG(ERROR) << "Failed to import lxd container. Empty response.";
    std::move(it->second).Run(CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED);
    import_lxd_container_callbacks_.erase(it);
    return;
  }

  // If import has started, the callback will be invoked when the
  // ImportLxdContainerProgressSignal signal indicates that import is
  // complete, otherwise this is an error.
  if (response->status() !=
      vm_tools::cicerone::ImportLxdContainerResponse::IMPORTING) {
    LOG(ERROR) << "Failed to import container: " << response->failure_reason();
    std::move(it->second).Run(CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED);
    import_lxd_container_callbacks_.erase(it);
  }
}

void CrostiniManager::OnImportLxdContainerProgress(
    const vm_tools::cicerone::ImportLxdContainerProgressSignal& signal) {
  if (signal.owner_id() != owner_id_)
    return;

  bool call_observers = false;
  bool call_original_callback = false;
  ImportContainerProgressStatus status;
  CrostiniResult result;
  switch (signal.status()) {
    case vm_tools::cicerone::ImportLxdContainerProgressSignal::IMPORTING_UPLOAD:
      call_observers = true;
      status = ImportContainerProgressStatus::UPLOAD;
      break;
    case vm_tools::cicerone::ImportLxdContainerProgressSignal::IMPORTING_UNPACK:
      call_observers = true;
      status = ImportContainerProgressStatus::UNPACK;
      break;
    case vm_tools::cicerone::ImportLxdContainerProgressSignal::CANCELLED:
      call_original_callback = true;
      result = CrostiniResult::CONTAINER_EXPORT_IMPORT_CANCELLED;
      break;
    case vm_tools::cicerone::ImportLxdContainerProgressSignal::DONE:
      call_original_callback = true;
      result = CrostiniResult::SUCCESS;
      break;
    case vm_tools::cicerone::ImportLxdContainerProgressSignal::
        FAILED_ARCHITECTURE:
      call_observers = true;
      status = ImportContainerProgressStatus::FAILURE_ARCHITECTURE;
      call_original_callback = true;
      result = CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_ARCHITECTURE;
      break;
    case vm_tools::cicerone::ImportLxdContainerProgressSignal::FAILED_SPACE:
      call_observers = true;
      status = ImportContainerProgressStatus::FAILURE_SPACE;
      call_original_callback = true;
      result = CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_SPACE;
      break;
    default:
      call_original_callback = true;
      result = CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED;
      LOG(ERROR) << "Failed during import container: " << signal.status()
                 << ", " << signal.failure_reason();
  }

  const ContainerId container_id(signal.vm_name(), signal.container_name());

  if (call_observers) {
    for (auto& observer : import_container_progress_observers_) {
      observer.OnImportContainerProgress(
          container_id, status, signal.progress_percent(),
          signal.progress_speed(), signal.architecture_device(),
          signal.architecture_container(), signal.available_space(),
          signal.min_required_space());
    }
  }

  // Invoke original callback with either success or failure.
  if (call_original_callback) {
    auto it = import_lxd_container_callbacks_.find(container_id);
    if (it == import_lxd_container_callbacks_.end()) {
      LOG(ERROR) << "No import callback for " << container_id;
      return;
    }
    std::move(it->second).Run(result);
    import_lxd_container_callbacks_.erase(it);
  }
}

void CrostiniManager::OnCancelExportLxdContainer(
    const ContainerId& key,
    base::Optional<vm_tools::cicerone::CancelExportLxdContainerResponse>
        response) {
  auto it = export_lxd_container_callbacks_.find(key);
  if (it == export_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "No export callback for " << key;
    return;
  }

  if (!response) {
    LOG(ERROR) << "Failed to cancel lxd container export. Empty response.";
    return;
  }

  if (response->status() !=
      vm_tools::cicerone::CancelExportLxdContainerResponse::CANCEL_QUEUED) {
    LOG(ERROR) << "Failed to cancel lxd container export:"
               << " status=" << response->status()
               << ", failure_reason=" << response->failure_reason();
  }
}

void CrostiniManager::OnCancelImportLxdContainer(
    const ContainerId& key,
    base::Optional<vm_tools::cicerone::CancelImportLxdContainerResponse>
        response) {
  auto it = import_lxd_container_callbacks_.find(key);
  if (it == import_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "No import callback for " << key;
    return;
  }

  if (!response) {
    LOG(ERROR) << "Failed to cancel lxd container import. Empty response.";
    return;
  }

  if (response->status() !=
      vm_tools::cicerone::CancelImportLxdContainerResponse::CANCEL_QUEUED) {
    LOG(ERROR) << "Failed to cancel lxd container import:"
               << " status=" << response->status()
               << ", failure_reason=" << response->failure_reason();
  }
}

void CrostiniManager::OnUpgradeContainer(
    CrostiniResultCallback callback,
    base::Optional<vm_tools::cicerone::UpgradeContainerResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to start upgrading container. Empty response";
    std::move(callback).Run(CrostiniResult::UPGRADE_CONTAINER_FAILED);
    return;
  }
  CrostiniResult result = CrostiniResult::SUCCESS;
  switch (response->status()) {
    case vm_tools::cicerone::UpgradeContainerResponse::STARTED:
      break;
    case vm_tools::cicerone::UpgradeContainerResponse::ALREADY_RUNNING:
      result = CrostiniResult::UPGRADE_CONTAINER_ALREADY_RUNNING;
      LOG(ERROR) << "Upgrade already running. Nothing to do.";
      break;
    case vm_tools::cicerone::UpgradeContainerResponse::ALREADY_UPGRADED:
      LOG(ERROR) << "Container already upgraded. Nothing to do.";
      result = CrostiniResult::UPGRADE_CONTAINER_ALREADY_UPGRADED;
      break;
    case vm_tools::cicerone::UpgradeContainerResponse::NOT_SUPPORTED:
      result = CrostiniResult::UPGRADE_CONTAINER_NOT_SUPPORTED;
      break;
    case vm_tools::cicerone::UpgradeContainerResponse::UNKNOWN:
    case vm_tools::cicerone::UpgradeContainerResponse::FAILED:
    default:
      LOG(ERROR) << "Upgrade container failed. Failure reason "
                 << response->failure_reason();
      result = CrostiniResult::UPGRADE_CONTAINER_FAILED;
      break;
  }
  std::move(callback).Run(result);
}

void CrostiniManager::OnCancelUpgradeContainer(
    CrostiniResultCallback callback,
    base::Optional<vm_tools::cicerone::CancelUpgradeContainerResponse>
        response) {
  if (!response) {
    LOG(ERROR) << "Failed to cancel upgrading container. Empty response";
    std::move(callback).Run(CrostiniResult::CANCEL_UPGRADE_CONTAINER_FAILED);
    return;
  }
  CrostiniResult result = CrostiniResult::SUCCESS;
  switch (response->status()) {
    case vm_tools::cicerone::CancelUpgradeContainerResponse::CANCELLED:
    case vm_tools::cicerone::CancelUpgradeContainerResponse::NOT_RUNNING:
      break;

    case vm_tools::cicerone::CancelUpgradeContainerResponse::UNKNOWN:
    case vm_tools::cicerone::CancelUpgradeContainerResponse::FAILED:
    default:
      LOG(ERROR) << "Cancel upgrade container failed. Failure reason "
                 << response->failure_reason();
      result = CrostiniResult::CANCEL_UPGRADE_CONTAINER_FAILED;
      break;
  }
  std::move(callback).Run(result);
}

void CrostiniManager::OnPendingAppListUpdates(
    const vm_tools::cicerone::PendingAppListUpdatesSignal& signal) {
  ContainerId container_id(signal.vm_name(), signal.container_name());
  for (auto& observer : pending_app_list_updates_observers_) {
    observer.OnPendingAppListUpdates(container_id, signal.count());
  }
}

void CrostiniManager::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  auto info =
      GetContainerInfo(kCrostiniDefaultVmName, kCrostiniDefaultContainerName);
  if (!info || !info->sshfs_mounted) {
    return;
  }

  // Block suspend and try to unmount sshfs (https://crbug.com/968060).
  auto token = base::UnguessableToken::Create();
  chromeos::PowerManagerClient::Get()->BlockSuspend(token, "CrostiniManager");
  file_manager::VolumeManager::Get(profile_)->RemoveSshfsCrostiniVolume(
      file_manager::util::GetCrostiniMountDirectory(profile_),
      base::BindOnce(&CrostiniManager::OnRemoveSshfsCrostiniVolume,
                     weak_ptr_factory_.GetWeakPtr(), token));
}

void CrostiniManager::SuspendDone(const base::TimeDelta& sleep_duration) {
  // https://crbug.com/968060.  Sshfs is unmounted before suspend,
  // call RestartCrostini to force remount if container is running.
  if (GetContainerInfo(kCrostiniDefaultVmName, kCrostiniDefaultContainerName)) {
    RestartCrostini(kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
                    base::DoNothing());
  }
}

void CrostiniManager::OnRemoveSshfsCrostiniVolume(
    base::UnguessableToken power_manager_suspend_token,
    bool result) {
  if (result) {
    SetContainerSshfsMounted(kCrostiniDefaultVmName,
                             kCrostiniDefaultContainerName, false);
  }
  // Need to let the device suspend after cleaning up.
  chromeos::PowerManagerClient::Get()->UnblockSuspend(
      power_manager_suspend_token);
}

void CrostiniManager::RemoveUncleanSshfsMounts() {
  file_manager::VolumeManager::Get(profile_)->RemoveSshfsCrostiniVolume(
      file_manager::util::GetCrostiniMountDirectory(profile_),
      base::DoNothing());
}

void CrostiniManager::DeallocateForwardedPortsCallback(
    Profile* profile,
    const ContainerId& container_id) {
  crostini::CrostiniPortForwarder::GetForProfile(profile)
      ->DeactivateAllActivePorts(container_id);
}

void CrostiniManager::AddCrostiniMicSharingEnabledObserver(
    CrostiniMicSharingEnabledObserver* observer) {
  crostini_mic_sharing_enabled_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveCrostiniMicSharingEnabledObserver(
    CrostiniMicSharingEnabledObserver* observer) {
  crostini_mic_sharing_enabled_observers_.RemoveObserver(observer);
}

void CrostiniManager::SetCrostiniMicSharingEnabled(bool enabled) {
  if (crostini_mic_sharing_enabled_ == enabled)
    return;
  crostini_mic_sharing_enabled_ = enabled;
  for (auto& observer : crostini_mic_sharing_enabled_observers_) {
    observer.OnCrostiniMicSharingEnabledChanged(crostini_mic_sharing_enabled_);
  }
}

}  // namespace crostini
