// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_manager.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/component_updater/cros_component_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/crostini/crostini_util.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/message.h"
#include "extensions/browser/extension_registry.h"
#include "net/base/escape.h"

namespace crostini {

namespace {

constexpr int64_t kMinimumDiskSize = 1ll * 1024 * 1024 * 1024;  // 1 GiB
constexpr base::FilePath::CharType kHomeDirectory[] =
    FILE_PATH_LITERAL("/home");

chromeos::ConciergeClient* GetConciergeClient() {
  return chromeos::DBusThreadManager::Get()->GetConciergeClient();
}

std::string ContainerUserNameForProfile(Profile* profile) {
  // Get rid of the @domain.name in the profile user name (an email address).
  std::string container_username = profile->GetProfileUserName();
  return container_username.substr(0, container_username.find('@'));
}

class CrostiniRestarter : public base::RefCountedThreadSafe<CrostiniRestarter> {
 public:
  CrostiniRestarter(std::string vm_name,
                    std::string cryptohome_id,
                    std::string container_name,
                    std::string container_username,
                    CrostiniManager::RestartCrostiniCallback callback)
      : vm_name_(std::move(vm_name)),
        cryptohome_id_(std::move(cryptohome_id)),
        container_name_(std::move(container_name)),
        container_username_(std::move(container_username)),
        callback_(std::move(callback)) {}

  void Restart() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    g_browser_process->platform_part()->cros_component_manager()->Load(
        "cros-termina",
        component_updater::CrOSComponentManager::MountPolicy::kMount,
        base::BindOnce(&CrostiniRestarter::InstallImageLoaderFinished,
                       base::WrapRefCounted(this)));
  }

 private:
  friend class base::RefCountedThreadSafe<CrostiniRestarter>;

  ~CrostiniRestarter() {
    if (callback_) {
      LOG(ERROR) << "Destroying without having called the callback.";
    }
  }

  static void InstallImageLoaderFinished(
      scoped_refptr<CrostiniRestarter> restarter,
      component_updater::CrOSComponentManager::Error error,
      const base::FilePath& result) {
    DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::BindOnce(&CrostiniRestarter::InstallImageLoaderFinishedOnUIThread,
                       restarter, error, result));
  }

  void InstallImageLoaderFinishedOnUIThread(
      component_updater::CrOSComponentManager::Error error,
      const base::FilePath& result) {
    if (error != component_updater::CrOSComponentManager::Error::NONE) {
      LOG(ERROR)
          << "Failed to install the cros-termina component with error code: "
          << static_cast<int>(error);
      std::move(callback_).Run(ConciergeClientResult::CONTAINER_START_FAILED);
      return;
    }
    CrostiniManager::GetInstance()->StartVmConcierge(
        base::BindOnce(&CrostiniRestarter::ConciergeStarted, this));
  }

  void ConciergeStarted(bool is_started) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!is_started) {
      LOG(ERROR) << "Failed to start Concierge service.";
      std::move(callback_).Run(ConciergeClientResult::CONTAINER_START_FAILED);
      return;
    }
    CrostiniManager::GetInstance()->CreateDiskImage(
        cryptohome_id_, base::FilePath(vm_name_),
        vm_tools::concierge::StorageLocation::STORAGE_CRYPTOHOME_ROOT,
        base::BindOnce(&CrostiniRestarter::CreateDiskImageFinished, this));
  }

  void CreateDiskImageFinished(ConciergeClientResult result,
                               const base::FilePath& result_path) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (result != ConciergeClientResult::SUCCESS) {
      LOG(ERROR) << "Failed to create disk image.";
      std::move(callback_).Run(result);
      return;
    }
    CrostiniManager::GetInstance()->StartTerminaVm(
        vm_name_, result_path,
        base::BindOnce(&CrostiniRestarter::StartTerminaVmFinished, this));
  }

  void StartTerminaVmFinished(ConciergeClientResult result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (result != ConciergeClientResult::SUCCESS) {
      LOG(ERROR) << "Failed to Start Termina VM.";
      std::move(callback_).Run(result);
      return;
    }
    CrostiniManager::GetInstance()->StartContainer(
        vm_name_, container_name_, container_username_,
        base::BindOnce(&CrostiniRestarter::StartContainerFinished, this));
  }

  void StartContainerFinished(ConciergeClientResult result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (result != ConciergeClientResult::SUCCESS) {
      LOG(ERROR) << "Failed to start container.";
    }
    std::move(callback_).Run(result);
  }

  std::string vm_name_;
  std::string cryptohome_id_;
  std::string container_name_;
  std::string container_username_;
  CrostiniManager::RestartCrostiniCallback callback_;
};

}  // namespace

// static
CrostiniManager* CrostiniManager::GetInstance() {
  return base::Singleton<CrostiniManager>::get();
}

CrostiniManager::CrostiniManager() : weak_ptr_factory_(this) {
  // ConciergeClient and its observer_list_ will be destroyed together.
  // We add, but don't need to remove the observer. (Doing so would force a
  // "destroyed before" dependency on the owner of ConciergeClient).
  GetConciergeClient()->AddObserver(this);
}

CrostiniManager::~CrostiniManager() {}

// static
bool CrostiniManager::IsCrosTerminaInstalled() {
  return !g_browser_process->platform_part()
              ->cros_component_manager()
              ->GetCompatiblePath("cros-termina")
              .empty();
}

void CrostiniManager::StartVmConcierge(StartVmConciergeCallback callback) {
  VLOG(1) << "Starting VmConcierge service";
  chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->StartVmConcierge(
      base::BindOnce(&CrostiniManager::OnStartVmConcierge,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::OnStartVmConcierge(StartVmConciergeCallback callback,
                                         bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to start Concierge service";
    std::move(callback).Run(success);
    return;
  }
  VLOG(1) << "VmConcierge service started";
  VLOG(1) << "Waiting for VmConcierge to announce availability.";

  GetConciergeClient()->WaitForServiceToBeAvailable(std::move(callback));
}

void CrostiniManager::CreateDiskImage(
    const std::string& cryptohome_id,
    const base::FilePath& disk_path,
    vm_tools::concierge::StorageLocation storage_location,
    CreateDiskImageCallback callback) {
  if (cryptohome_id.empty()) {
    LOG(ERROR) << "Cryptohome id cannot be empty";
    std::move(callback).Run(ConciergeClientResult::CLIENT_ERROR,
                            base::FilePath());
    return;
  }

  std::string disk_path_string = disk_path.AsUTF8Unsafe();
  if (disk_path_string.empty()) {
    LOG(ERROR) << "Disk path cannot be empty";
    std::move(callback).Run(ConciergeClientResult::CLIENT_ERROR,
                            base::FilePath());
    return;
  }

  vm_tools::concierge::CreateDiskImageRequest request;
  request.set_cryptohome_id(std::move(cryptohome_id));
  request.set_disk_path(std::move(disk_path_string));
  // The type of disk image to be created.
  request.set_image_type(vm_tools::concierge::DISK_IMAGE_QCOW2);

  if (storage_location != vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT &&
      storage_location != vm_tools::concierge::STORAGE_CRYPTOHOME_DOWNLOADS) {
    LOG(ERROR) << "'" << storage_location
               << "' is not a valid storage location";
    std::move(callback).Run(ConciergeClientResult::CLIENT_ERROR,
                            base::FilePath());
    return;
  }
  request.set_storage_location(storage_location);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                     base::FilePath(kHomeDirectory)),
      base::BindOnce(&CrostiniManager::CreateDiskImageAfterSizeCheck,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     std::move(callback)));
}

void CrostiniManager::CreateDiskImageAfterSizeCheck(
    vm_tools::concierge::CreateDiskImageRequest request,
    CreateDiskImageCallback callback,
    int64_t free_disk_size) {
  int64_t disk_size = (free_disk_size * 9) / 10;
  // Skip disk size check on dev box or trybots because
  // base::SysInfo::AmountOfFreeDiskSpace returns zero in testing.
  if (disk_size < kMinimumDiskSize && base::SysInfo::IsRunningOnChromeOS()) {
    LOG(ERROR) << "Insufficient disk available. Need to free "
               << kMinimumDiskSize - disk_size << " bytes";
    std::move(callback).Run(ConciergeClientResult::CLIENT_ERROR,
                            base::FilePath());
    return;
  }
  // The logical size of the new disk image, in bytes.
  request.set_disk_size(std::move(disk_size));

  GetConciergeClient()->CreateDiskImage(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnCreateDiskImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::DestroyDiskImage(
    const std::string& cryptohome_id,
    const base::FilePath& disk_path,
    vm_tools::concierge::StorageLocation storage_location,
    DestroyDiskImageCallback callback) {
  if (cryptohome_id.empty()) {
    LOG(ERROR) << "Cryptohome id cannot be empty";
    std::move(callback).Run(ConciergeClientResult::CLIENT_ERROR);
    return;
  }

  std::string disk_path_string = disk_path.AsUTF8Unsafe();
  if (disk_path_string.empty()) {
    LOG(ERROR) << "Disk path cannot be empty";
    std::move(callback).Run(ConciergeClientResult::CLIENT_ERROR);
    return;
  }

  vm_tools::concierge::DestroyDiskImageRequest request;
  request.set_cryptohome_id(std::move(cryptohome_id));
  request.set_disk_path(std::move(disk_path_string));

  if (storage_location != vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT &&
      storage_location != vm_tools::concierge::STORAGE_CRYPTOHOME_DOWNLOADS) {
    LOG(ERROR) << "'" << storage_location
               << "' is not a valid storage location";
    std::move(callback).Run(ConciergeClientResult::CLIENT_ERROR);
    return;
  }
  request.set_storage_location(storage_location);

  GetConciergeClient()->DestroyDiskImage(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnDestroyDiskImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::StartTerminaVm(std::string name,
                                     const base::FilePath& disk_path,
                                     StartTerminaVmCallback callback) {
  if (name.empty()) {
    LOG(ERROR) << "name is required";
    std::move(callback).Run(ConciergeClientResult::CLIENT_ERROR);
    return;
  }

  std::string disk_path_string = disk_path.AsUTF8Unsafe();
  if (disk_path_string.empty()) {
    LOG(ERROR) << "Disk path cannot be empty";
    std::move(callback).Run(ConciergeClientResult::CLIENT_ERROR);
    return;
  }

  vm_tools::concierge::StartVmRequest request;
  request.set_name(std::move(name));
  request.set_start_termina(true);

  vm_tools::concierge::DiskImage* disk_image = request.add_disks();
  disk_image->set_path(std::move(disk_path_string));
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_QCOW2);
  disk_image->set_writable(true);
  disk_image->set_do_mount(false);

  GetConciergeClient()->StartTerminaVm(
      request,
      base::BindOnce(&CrostiniManager::OnStartTerminaVm,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::StopVm(std::string name, StopVmCallback callback) {
  if (name.empty()) {
    LOG(ERROR) << "name is required";
    std::move(callback).Run(ConciergeClientResult::CLIENT_ERROR);
    return;
  }

  vm_tools::concierge::StopVmRequest request;
  request.set_name(std::move(name));

  GetConciergeClient()->StopVm(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnStopVm, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void CrostiniManager::StartContainer(std::string vm_name,
                                     std::string container_name,
                                     std::string container_username,
                                     StartContainerCallback callback) {
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(ConciergeClientResult::CLIENT_ERROR);
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(ConciergeClientResult::CLIENT_ERROR);
    return;
  }
  if (container_username.empty()) {
    LOG(ERROR) << "container_username is required";
    std::move(callback).Run(ConciergeClientResult::CLIENT_ERROR);
    return;
  }
  if (!GetConciergeClient()->IsContainerStartedSignalConnected()) {
    LOG(ERROR) << "Async call to StartContainer can't complete when signal "
                  "is not connected.";
    std::move(callback).Run(ConciergeClientResult::CLIENT_ERROR);
    return;
  }
  vm_tools::concierge::StartContainerRequest request;
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_container_username(std::move(container_username));
  request.set_async(true);

  GetConciergeClient()->StartContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnStartContainer,
                     weak_ptr_factory_.GetWeakPtr(), request.vm_name(),
                     request.container_name(), std::move(callback)));
}

void CrostiniManager::LaunchContainerApplication(
    std::string vm_name,
    std::string container_name,
    std::string desktop_file_id,
    LaunchContainerApplicationCallback callback) {
  vm_tools::concierge::LaunchContainerApplicationRequest request;
  request.set_vm_name(std::move(vm_name));
  request.set_container_name(std::move(container_name));
  request.set_desktop_file_id(std::move(desktop_file_id));

  GetConciergeClient()->LaunchContainerApplication(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnLaunchContainerApplication,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::LaunchContainerTerminal(
    Profile* profile,
    const std::string& vm_name,
    const std::string& container_name) {
  std::string container_username = ContainerUserNameForProfile(profile);
  std::string vsh_crosh = base::StringPrintf(
      "chrome-extension://%s/html/crosh.html?command=vmshell",
      kCrostiniCroshBuiltinAppId);
  std::string vm_name_param = net::EscapeQueryParamValue(
      base::StringPrintf("--vm_name=%s", vm_name.c_str()), false);
  std::string lxd_dir =
      net::EscapeQueryParamValue("LXD_DIR=/mnt/stateful/lxd", false);
  std::string lxd_conf =
      net::EscapeQueryParamValue("LXD_CONF=/mnt/stateful/lxd_conf", false);
  const extensions::Extension* crosh_extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          kCrostiniCroshBuiltinAppId);

  std::vector<base::StringPiece> pieces = {vsh_crosh,
                                           vm_name_param,
                                           "--",
                                           lxd_dir,
                                           lxd_conf,
                                           "run_container.sh",
                                           "--container_name",
                                           container_name,
                                           "--user",
                                           container_username,
                                           "--shell"};

  GURL vsh_in_crosh_url(base::JoinString(pieces, "&args[]="));

  AppLaunchParams launch_params(
      profile, crosh_extension, extensions::LAUNCH_CONTAINER_WINDOW,
      WindowOpenDisposition::NEW_WINDOW, extensions::SOURCE_APP_LAUNCHER);

  OpenApplicationWindow(launch_params, vsh_in_crosh_url);
}

void CrostiniManager::RestartCrostini(Profile* profile,
                                      std::string vm_name,
                                      std::string container_name,
                                      RestartCrostiniCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string cryptohome_id = chromeos::ProfileHelper::Get()
                                  ->GetUserByProfile(profile)
                                  ->username_hash();
  std::string container_username = ContainerUserNameForProfile(profile);

  auto crostini_restarter = base::MakeRefCounted<CrostiniRestarter>(
      std::move(vm_name), std::move(cryptohome_id), std::move(container_name),
      std::move(container_username), std::move(callback));
  crostini_restarter->Restart();
}

void CrostiniManager::OnCreateDiskImage(
    CreateDiskImageCallback callback,
    base::Optional<vm_tools::concierge::CreateDiskImageResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to create disk image. Empty response.";
    std::move(callback).Run(ConciergeClientResult::CREATE_DISK_IMAGE_FAILED,
                            base::FilePath());
    return;
  }
  vm_tools::concierge::CreateDiskImageResponse response = reply.value();

  if (response.status() != vm_tools::concierge::DISK_STATUS_EXISTS &&
      response.status() != vm_tools::concierge::DISK_STATUS_CREATED) {
    LOG(ERROR) << "Failed to create disk image: " << response.failure_reason();
    std::move(callback).Run(ConciergeClientResult::CREATE_DISK_IMAGE_FAILED,
                            base::FilePath());
    return;
  }

  std::move(callback).Run(ConciergeClientResult::SUCCESS,
                          base::FilePath(response.disk_path()));
}

void CrostiniManager::OnDestroyDiskImage(
    DestroyDiskImageCallback callback,
    base::Optional<vm_tools::concierge::DestroyDiskImageResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to destroy disk image. Empty response.";
    std::move(callback).Run(ConciergeClientResult::DESTROY_DISK_IMAGE_FAILED);
    return;
  }
  vm_tools::concierge::DestroyDiskImageResponse response =
      std::move(reply).value();

  if (response.status() != vm_tools::concierge::DISK_STATUS_DESTROYED &&
      response.status() != vm_tools::concierge::DISK_STATUS_DOES_NOT_EXIST) {
    LOG(ERROR) << "Failed to destroy disk image: " << response.failure_reason();
    std::move(callback).Run(ConciergeClientResult::DESTROY_DISK_IMAGE_FAILED);
    return;
  }

  std::move(callback).Run(ConciergeClientResult::SUCCESS);
}

void CrostiniManager::OnStartTerminaVm(
    StartTerminaVmCallback callback,
    base::Optional<vm_tools::concierge::StartVmResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to start termina vm. Empty response.";
    std::move(callback).Run(ConciergeClientResult::VM_START_FAILED);
    return;
  }
  vm_tools::concierge::StartVmResponse response = reply.value();

  if (!response.success()) {
    LOG(ERROR) << "Failed to start VM: " << response.failure_reason();
    std::move(callback).Run(ConciergeClientResult::VM_START_FAILED);
    return;
  }
  std::move(callback).Run(ConciergeClientResult::SUCCESS);
}

void CrostiniManager::OnStopVm(
    StopVmCallback callback,
    base::Optional<vm_tools::concierge::StopVmResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to stop termina vm. Empty response.";
    std::move(callback).Run(ConciergeClientResult::VM_STOP_FAILED);
    return;
  }
  vm_tools::concierge::StopVmResponse response = reply.value();

  if (!response.success()) {
    LOG(ERROR) << "Failed to stop VM: " << response.failure_reason();
    std::move(callback).Run(ConciergeClientResult::VM_STOP_FAILED);
    return;
  }

  std::move(callback).Run(ConciergeClientResult::SUCCESS);
}

void CrostiniManager::OnStartContainer(
    std::string vm_name,
    std::string container_name,
    StartContainerCallback callback,
    base::Optional<vm_tools::concierge::StartContainerResponse> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to start container in vm. Empty response.";
    std::move(callback).Run(ConciergeClientResult::CONTAINER_START_FAILED);
    return;
  }
  vm_tools::concierge::StartContainerResponse response = reply.value();
  if (response.status() == vm_tools::concierge::CONTAINER_STATUS_STARTING) {
    // The callback will be called when we receive the ContainerStated signal.
    start_container_callbacks_.emplace(std::make_pair(vm_name, container_name),
                                       std::move(callback));
    return;
  }
  if (response.status() != vm_tools::concierge::CONTAINER_STATUS_RUNNING) {
    LOG(ERROR) << "Failed to start container: " << response.failure_reason();
    std::move(callback).Run(ConciergeClientResult::CONTAINER_START_FAILED);
    return;
  }
  std::move(callback).Run(ConciergeClientResult::SUCCESS);
}

void CrostiniManager::OnContainerStarted(
    const vm_tools::concierge::ContainerStartedSignal& signal) {
  // Find the callbacks to call, then erase them from the map.
  auto range = start_container_callbacks_.equal_range(
      std::make_pair(signal.vm_name(), signal.container_name()));
  for (auto it = range.first; it != range.second; it++) {
    std::move(it->second).Run(ConciergeClientResult::SUCCESS);
  }
  start_container_callbacks_.erase(range.first, range.second);
}

void CrostiniManager::OnLaunchContainerApplication(
    LaunchContainerApplicationCallback callback,
    base::Optional<vm_tools::concierge::LaunchContainerApplicationResponse>
        reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to launch application. Empty response.";
    std::move(callback).Run(
        ConciergeClientResult::LAUNCH_CONTAINER_APPLICATION_FAILED);
    return;
  }
  vm_tools::concierge::LaunchContainerApplicationResponse response =
      reply.value();

  if (!response.success()) {
    LOG(ERROR) << "Failed to launch application: " << response.failure_reason();
    std::move(callback).Run(
        ConciergeClientResult::LAUNCH_CONTAINER_APPLICATION_FAILED);
    return;
  }

  std::move(callback).Run(ConciergeClientResult::SUCCESS);
}

}  // namespace crostini
