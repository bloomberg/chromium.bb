// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_vm_client_adapter.h"

#include <sys/statvfs.h>
#include <time.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/login_manager/arc.pb.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "chromeos/system/statistics_provider.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_util.h"

namespace arc {
namespace {

constexpr const char kArcVmConfigJsonPath[] = "/usr/share/arcvm/config.json";
constexpr const char kArcVmServerProxyJobName[] = "arcvm_2dserver_2dproxy";
constexpr const char kBuiltinPath[] = "/opt/google/vms/android";
constexpr const char kCrosSystemPath[] = "/usr/bin/crossystem";
constexpr const char kDlcPath[] = "/run/imageloader/arcvm-dlc/package/root";
constexpr const char kFstab[] = "fstab";
constexpr const char kHomeDirectory[] = "/home";
constexpr const char kKernel[] = "vmlinux";
constexpr const char kRootFs[] = "system.raw.img";
constexpr const char kVendorImage[] = "vendor.raw.img";

// A move-only class to hold status of the host file system.
class FileSystemStatus {
 public:
  FileSystemStatus(FileSystemStatus&& rhs) = default;
  ~FileSystemStatus() = default;
  FileSystemStatus& operator=(FileSystemStatus&& rhs) = default;

  bool is_android_debuggable() const { return is_android_debuggable_; }
  bool is_host_rootfs_writable() const { return is_host_rootfs_writable_; }
  const base::FilePath& system_image_path() const { return system_image_path_; }
  const base::FilePath& vendor_image_path() const { return vendor_image_path_; }
  const base::FilePath& guest_kernel_path() const { return guest_kernel_path_; }
  const base::FilePath& fstab_path() const { return fstab_path_; }

  static FileSystemStatus GetFileSystemStatusBlocking() {
    return FileSystemStatus();
  }
  static bool IsAndroidDebuggableForTesting(const base::FilePath& json_path) {
    return IsAndroidDebuggable(json_path);
  }

 private:
  FileSystemStatus()
      : is_android_debuggable_(
            IsAndroidDebuggable(base::FilePath(kArcVmConfigJsonPath))),
        is_host_rootfs_writable_(IsHostRootfsWritable()),
        system_image_path_(SelectDlcOrBuiltin(base::FilePath(kRootFs))),
        vendor_image_path_(SelectDlcOrBuiltin(base::FilePath(kVendorImage))),
        guest_kernel_path_(SelectDlcOrBuiltin(base::FilePath(kKernel))),
        fstab_path_(SelectDlcOrBuiltin(base::FilePath(kFstab))) {}

  // Parse a JSON file which is like the following and returns a result:
  //   {
  //     "ANDROID_DEBUGGABLE": false
  //   }
  static bool IsAndroidDebuggable(const base::FilePath& json_path) {
    // TODO(yusukes): Remove this fallback after adding the json file.
    if (!base::PathExists(json_path))
      return true;

    std::string content;
    if (!base::ReadFileToString(json_path, &content))
      return false;

    base::JSONReader::ValueWithError result(
        base::JSONReader::ReadAndReturnValueWithError(content,
                                                      base::JSON_PARSE_RFC));
    if (!result.value) {
      LOG(ERROR) << "Error parsing " << json_path
                 << ": code=" << result.error_code
                 << ", message=" << result.error_message << ": " << content;
      return false;
    }
    if (!result.value->is_dict()) {
      LOG(ERROR) << "Error parsing " << json_path << ": " << *(result.value);
      return false;
    }

    const base::Value* debuggable = result.value->FindKeyOfType(
        "ANDROID_DEBUGGABLE", base::Value::Type::BOOLEAN);
    if (!debuggable) {
      LOG(ERROR) << "ANDROID_DEBUGGABLE is not found in " << json_path;
      return false;
    }

    return debuggable->GetBool();
  }

  static bool IsHostRootfsWritable() {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    struct statvfs buf;
    if (statvfs("/", &buf) < 0) {
      PLOG(ERROR) << "statvfs() failed";
      return false;
    }
    const bool rw = !(buf.f_flag & ST_RDONLY);
    VLOG(1) << "Host's rootfs is " << (rw ? "rw" : "ro");
    return rw;
  }

  static base::FilePath SelectDlcOrBuiltin(const base::FilePath& file) {
    const base::FilePath dlc_path = base::FilePath(kDlcPath).Append(file);
    if (base::PathExists(dlc_path)) {
      VLOG(1) << "arcvm-dlc will be used for " << file.value();
      return dlc_path;
    }
    return base::FilePath(kBuiltinPath).Append(file);
  }

  bool is_android_debuggable_;
  bool is_host_rootfs_writable_;
  base::FilePath system_image_path_;
  base::FilePath vendor_image_path_;
  base::FilePath guest_kernel_path_;
  base::FilePath fstab_path_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemStatus);
};

chromeos::ConciergeClient* GetConciergeClient() {
  return chromeos::DBusThreadManager::Get()->GetConciergeClient();
}

// TODO(pliard): Export host-side /data to the VM, and remove the function.
vm_tools::concierge::CreateDiskImageRequest CreateArcDiskRequest(
    const std::string& user_id_hash,
    int64_t free_disk_bytes) {
  DCHECK(!user_id_hash.empty());

  vm_tools::concierge::CreateDiskImageRequest request;
  request.set_cryptohome_id(user_id_hash);
  request.set_disk_path("arcvm");

  // The type of disk image to be created.
  request.set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);

  // The logical size of the new disk image, in bytes.
  request.set_disk_size(free_disk_bytes / 2);

  return request;
}

std::string GetReleaseChannel() {
  constexpr const char kUnknown[] = "unknown";
  const std::set<std::string> channels = {"beta",    "canary", "dev",
                                          "dogfood", "stable", "testimage"};

  std::string value;
  if (!base::SysInfo::GetLsbReleaseValue("CHROMEOS_RELEASE_TRACK", &value)) {
    LOG(ERROR) << "Could not load lsb-release";
    return kUnknown;
  }

  auto pieces = base::SplitString(value, "-", base::KEEP_WHITESPACE,
                                  base::SPLIT_WANT_ALL);

  if (pieces.size() != 2 || pieces[1] != "channel") {
    LOG(ERROR) << "Misformatted CHROMEOS_RELEASE_TRACK value in lsb-release";
    return kUnknown;
  }
  if (channels.find(pieces[0]) == channels.end()) {
    LOG(WARNING) << "Unknown ChromeOS channel: \"" << pieces[0] << "\"";
    return kUnknown;
  }

  return pieces[0];
}

std::string MonotonicTimestamp() {
  struct timespec ts;
  const int ret = clock_gettime(CLOCK_BOOTTIME, &ts);
  DPCHECK(ret == 0);
  const int64_t time =
      ts.tv_sec * base::Time::kNanosecondsPerSecond + ts.tv_nsec;
  return base::NumberToString(time);
}

std::vector<std::string> GenerateKernelCmdline(
    int32_t lcd_density,
    const base::Optional<bool>& play_store_auto_update,
    const FileSystemStatus& file_system_status,
    bool is_dev_mode,
    bool is_host_on_vm) {
  const std::string release_channel = GetReleaseChannel();
  const bool stable_or_beta =
      release_channel == "stable" || release_channel == "beta";

  std::vector<std::string> result = {
      "androidboot.hardware=bertha",
      "androidboot.container=1",
      // TODO(b/139480143): when ArcNativeBridgeExperiment is enabled, switch
      // to ndk_translation.
      "androidboot.native_bridge=libhoudini.so",
      base::StringPrintf("androidboot.dev_mode=%d", is_dev_mode),
      base::StringPrintf("androidboot.disable_runas=%d", !is_dev_mode),
      base::StringPrintf("androidboot.vm=%d", is_host_on_vm),
      base::StringPrintf("androidboot.debuggable=%d",
                         file_system_status.is_android_debuggable()),
      base::StringPrintf("androidboot.lcd_density=%d", lcd_density),
      base::StringPrintf(
          "androidboot.arc_file_picker=%d",
          base::FeatureList::IsEnabled(kFilePickerExperimentFeature)),
      base::StringPrintf(
          "androidboot.arc_custom_tabs=%d",
          base::FeatureList::IsEnabled(kCustomTabsExperimentFeature) &&
              !stable_or_beta),
      base::StringPrintf(
          "androidboot.arc_print_spooler=%d",
          base::FeatureList::IsEnabled(kPrintSpoolerExperimentFeature) &&
              !stable_or_beta),
      "androidboot.chromeos_channel=" + release_channel,
      "androidboot.boottime_offset=" + MonotonicTimestamp(),
      // TODO(yusukes): remove this once arcvm supports SELinux.
      "androidboot.selinux=permissive",
  };

  if (play_store_auto_update) {
    result.push_back(base::StringPrintf("androidboot.play_store_auto_update=%d",
                                        *play_store_auto_update));
  }

  // TODO(yusukes): include command line parameters from arcbootcontinue.
  return result;
}

vm_tools::concierge::StartArcVmRequest CreateStartArcVmRequest(
    const std::string& user_id_hash,
    uint32_t cpus,
    const base::FilePath& data_disk_path,
    const FileSystemStatus& file_system_status,
    std::vector<std::string> kernel_cmdline) {
  vm_tools::concierge::StartArcVmRequest request;

  request.set_name(kArcVmName);
  request.set_owner_id(user_id_hash);

  request.add_params("root=/dev/vda");
  if (file_system_status.is_host_rootfs_writable())
    request.add_params("rw");
  request.add_params("init=/init");
  for (auto& entry : kernel_cmdline)
    request.add_params(std::move(entry));

  vm_tools::concierge::VirtualMachineSpec* vm = request.mutable_vm();

  vm->set_kernel(file_system_status.guest_kernel_path().value());

  // Add / as /dev/vda.
  vm->set_rootfs(file_system_status.system_image_path().value());

  // Add /data as /dev/vdb.
  vm_tools::concierge::DiskImage* disk_image = request.add_disks();
  disk_image->set_path(data_disk_path.value());
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(true);
  disk_image->set_do_mount(true);
  // Add /vendor as /dev/vdc.
  disk_image = request.add_disks();
  disk_image->set_path(file_system_status.vendor_image_path().value());

  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(false);
  disk_image->set_do_mount(true);

  // Add Android fstab.
  request.set_fstab(file_system_status.fstab_path().value());

  // Add cpus.
  request.set_cpus(cpus);

  return request;
}

// Gets a system property managed by crossystem. This function can be called
// only with base::MayBlock().
int GetSystemPropertyInt(const std::string& property) {
  std::string output;
  if (!base::GetAppOutput({kCrosSystemPath, property}, &output))
    return -1;
  int output_int;
  return base::StringToInt(output, &output_int) ? output_int : -1;
}

}  // namespace

class ArcVmClientAdapter : public ArcClientAdapter,
                           public chromeos::ConciergeClient::VmObserver,
                           public chromeos::ConciergeClient::Observer {
 public:
  // Initializing |is_host_on_vm_| and |is_dev_mode_| is not always very fast.
  // Try to initialize them in the constructor and in StartMiniArc respectively.
  // They usually run when the system is not busy.
  ArcVmClientAdapter()
      : is_host_on_vm_(chromeos::system::StatisticsProvider::GetInstance()
                           ->IsRunningOnVm()) {
    auto* client = GetConciergeClient();
    client->AddVmObserver(this);
    client->AddObserver(this);
  }

  ~ArcVmClientAdapter() override {
    auto* client = GetConciergeClient();
    client->RemoveObserver(this);
    client->RemoveVmObserver(this);
  }

  // chromeos::ConciergeClient::VmObserver overrides:
  void OnVmStarted(
      const vm_tools::concierge::VmStartedSignal& signal) override {
    if (signal.name() == kArcVmName)
      VLOG(1) << "OnVmStarted: ARCVM cid=" << signal.vm_info().cid();
  }

  void OnVmStopped(
      const vm_tools::concierge::VmStoppedSignal& signal) override {
    if (signal.name() != kArcVmName)
      return;
    VLOG(1) << "OnVmStopped: ARCVM";
    OnArcInstanceStopped();
  }

  // ArcClientAdapter overrides:
  void StartMiniArc(const StartArcMiniContainerRequest& request,
                    chromeos::VoidDBusMethodCallback callback) override {
    // TODO(yusukes): Support mini ARC.
    VLOG(2) << "Mini ARCVM instance is not supported.";
    // Save the lcd density and auto update mode for the later call to
    // UpgradeArc.
    lcd_density_ = request.lcd_density();
    cpus_ = base::SysInfo::NumberOfProcessors() - request.num_cores_disabled();
    DCHECK_LT(0u, cpus_);
    if (request.play_store_auto_update() ==
        login_manager::
            StartArcMiniContainerRequest_PlayStoreAutoUpdate_AUTO_UPDATE_ON) {
      play_store_auto_update_ = true;
    } else if (
        request.play_store_auto_update() ==
        login_manager::
            StartArcMiniContainerRequest_PlayStoreAutoUpdate_AUTO_UPDATE_OFF) {
      play_store_auto_update_ = false;
    }
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskPriority::USER_VISIBLE},
        base::BindOnce(
            []() { return GetSystemPropertyInt("cros_debug") == 1; }),
        base::BindOnce(&ArcVmClientAdapter::OnIsDevMode,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UpgradeArc(const UpgradeArcContainerRequest& request,
                  chromeos::VoidDBusMethodCallback callback) override {
    VLOG(1) << "Starting arcvm-server-proxy";
    chromeos::UpstartClient::Get()->StartJob(
        kArcVmServerProxyJobName, /*environment=*/{},
        base::BindOnce(&ArcVmClientAdapter::OnArcVmServerProxyJobStarted,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StopArcInstance() override {
    VLOG(1) << "Stopping arcvm";
    vm_tools::concierge::StopVmRequest request;
    request.set_name(kArcVmName);
    request.set_owner_id(user_id_hash_);
    GetConciergeClient()->StopVm(
        request, base::BindOnce(&ArcVmClientAdapter::OnStopVmReply,
                                weak_factory_.GetWeakPtr()));
  }

  void SetUserIdHashForProfile(const std::string& hash) override {
    DCHECK(!hash.empty());
    user_id_hash_ = hash;
  }

  // chromeos::ConciergeClient::Observer overrides:
  void ConciergeServiceStopped() override {
    VLOG(1) << "vm_concierge stopped";
    // At this point, all crosvm processes are gone. Notify the observer of the
    // event.
    OnArcInstanceStopped();
  }

  void ConciergeServiceRestarted() override {}

 private:
  void OnIsDevMode(chromeos::VoidDBusMethodCallback callback,
                   bool is_dev_mode) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    is_dev_mode_ = is_dev_mode;
    should_notify_observers_ = true;
  }

  void OnConciergeStarted(chromeos::VoidDBusMethodCallback callback,
                          bool success) {
    if (!success) {
      LOG(ERROR) << "Failed to start Concierge service for arcvm";
      std::move(callback).Run(false);
      return;
    }
    VLOG(1) << "Concierge service started for arcvm.";

    // TODO(pliard): Export host-side /data to the VM, and remove the call. Note
    // that ArcSessionImpl checks low disk conditions before calling UpgradeArc.
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                       base::FilePath(kHomeDirectory)),
        base::BindOnce(&ArcVmClientAdapter::CreateDiskImageAfterSizeCheck,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CreateDiskImageAfterSizeCheck(chromeos::VoidDBusMethodCallback callback,
                                     int64_t free_disk_bytes) {
    VLOG(2) << "Got free disk size: " << free_disk_bytes;
    if (user_id_hash_.empty()) {
      LOG(ERROR) << "User ID hash is not set";
      std::move(callback).Run(false);
      return;
    }
    // TODO(pliard): Export host-side /data to the VM, and remove the call.
    GetConciergeClient()->CreateDiskImage(
        CreateArcDiskRequest(user_id_hash_, free_disk_bytes),
        base::BindOnce(&ArcVmClientAdapter::OnDiskImageCreated,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  // TODO(pliard): Export host-side /data to the VM, and remove the first half
  // of the function.
  void OnDiskImageCreated(
      chromeos::VoidDBusMethodCallback callback,
      base::Optional<vm_tools::concierge::CreateDiskImageResponse> reply) {
    if (!reply.has_value()) {
      LOG(ERROR) << "Failed to create disk image. Empty response.";
      std::move(callback).Run(false);
      return;
    }

    const vm_tools::concierge::CreateDiskImageResponse& response =
        reply.value();
    if (response.status() != vm_tools::concierge::DISK_STATUS_EXISTS &&
        response.status() != vm_tools::concierge::DISK_STATUS_CREATED) {
      LOG(ERROR) << "Failed to create disk image: "
                 << response.failure_reason();
      std::move(callback).Run(false);
      return;
    }
    VLOG(1) << "Disk image for arcvm ready. status=" << response.status()
            << ", disk=" << response.disk_path();

    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&FileSystemStatus::GetFileSystemStatusBlocking),
        base::BindOnce(&ArcVmClientAdapter::OnFileSystemStatus,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       base::FilePath(response.disk_path())));
  }

  void OnFileSystemStatus(chromeos::VoidDBusMethodCallback callback,
                          const base::FilePath& data_disk_path,
                          FileSystemStatus file_system_status) {
    VLOG(2) << "Got file system status";
    DCHECK(is_dev_mode_);
    std::vector<std::string> kernel_cmdline = GenerateKernelCmdline(
        lcd_density_, play_store_auto_update_, file_system_status,
        *is_dev_mode_, is_host_on_vm_);
    auto start_request =
        CreateStartArcVmRequest(user_id_hash_, cpus_, data_disk_path,
                                file_system_status, std::move(kernel_cmdline));
    GetConciergeClient()->StartArcVm(
        start_request,
        base::BindOnce(&ArcVmClientAdapter::OnStartArcVmReply,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnStartArcVmReply(
      chromeos::VoidDBusMethodCallback callback,
      base::Optional<vm_tools::concierge::StartVmResponse> reply) {
    if (!reply.has_value()) {
      LOG(ERROR) << "Failed to start arcvm. Empty response.";
      std::move(callback).Run(false);
      return;
    }

    const vm_tools::concierge::StartVmResponse& response = reply.value();
    if (response.status() != vm_tools::concierge::VM_STATUS_RUNNING) {
      LOG(ERROR) << "Failed to start arcvm: status=" << response.status()
                 << ", reason=" << response.failure_reason();
      std::move(callback).Run(false);
      return;
    }
    VLOG(1) << "arcvm started.";
    std::move(callback).Run(true);
  }

  void OnArcInstanceStopped() {
    VLOG(1) << "ARCVM stopped. Stopping arcvm-server-proxy";

    // TODO(yusukes): Consider removing this stop call once b/142140355 is
    // implemented.
    chromeos::UpstartClient::Get()->StopJob(
        kArcVmServerProxyJobName, /*environment=*/{},
        base::BindOnce(&ArcVmClientAdapter::OnArcVmServerProxyJobStopped,
                       weak_factory_.GetWeakPtr()));

    // If this method is called before even mini VM is started (e.g. very early
    // vm_concierge crash), or this method is called twice (e.g. crosvm crash
    // followed by vm_concierge crash), do nothing.
    if (!should_notify_observers_)
      return;
    should_notify_observers_ = false;

    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped();
  }

  void OnStopVmReply(
      base::Optional<vm_tools::concierge::StopVmResponse> reply) {
    // If the reply indicates the D-Bus call is successfully done, do nothing.
    // Concierge call OnVmStopped() eventually.
    if (reply.has_value() && reply.value().success())
      return;

    // We likely tried to stop mini VM which doesn't exist today. Notify
    // observers.
    // TODO(yusukes): Remove the fallback once we implement mini VM.
    OnArcInstanceStopped();
  }

  void OnArcVmServerProxyJobStarted(chromeos::VoidDBusMethodCallback callback,
                                    bool result) {
    if (!result) {
      LOG(ERROR) << "Failed to start arcvm-server-proxy job";
      std::move(callback).Run(false);
      return;
    }

    VLOG(1) << "Starting Concierge service";
    chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->StartConcierge(
        base::BindOnce(&ArcVmClientAdapter::OnConciergeStarted,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnArcVmServerProxyJobStopped(bool result) {
    VLOG(1) << "OnArcVmServerProxyJobStopped result=" << result;
  }

  base::Optional<bool> is_dev_mode_;
  // True when the *host* is running on a VM.
  const bool is_host_on_vm_;

  // A hash of the primary profile user ID.
  std::string user_id_hash_;

  int32_t lcd_density_;
  uint32_t cpus_;
  base::Optional<bool> play_store_auto_update_;

  bool should_notify_observers_ = false;

  // For callbacks.
  base::WeakPtrFactory<ArcVmClientAdapter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcVmClientAdapter);
};

std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapter() {
  return std::make_unique<ArcVmClientAdapter>();
}

bool IsAndroidDebuggableForTesting(const base::FilePath& json_path) {
  return FileSystemStatus::IsAndroidDebuggableForTesting(json_path);
}

}  // namespace arc
