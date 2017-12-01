// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_sandbox_hook_linux.h"

#include <dlfcn.h>
#include <errno.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_file.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "content/public/common/content_switches.h"
#include "media/gpu/features.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "services/service_manager/embedder/set_process_title.h"
#include "services/service_manager/sandbox/linux/bpf_cros_amd_gpu_policy_linux.h"
#include "services/service_manager/sandbox/linux/bpf_cros_arm_gpu_policy_linux.h"
#include "services/service_manager/sandbox/linux/bpf_gpu_policy_linux.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"

#if BUILDFLAG(USE_VAAPI)
#include <va/va_version.h>
#endif

using sandbox::bpf_dsl::Policy;
using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::BrokerProcess;

namespace content {
namespace {

inline bool IsChromeOS() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureX86_64() {
#if defined(__x86_64__)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureI386() {
#if defined(__i386__)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureArm() {
#if defined(ARCH_CPU_ARM_FAMILY)
  return true;
#else
  return false;
#endif
}

inline bool UseV4L2Codec() {
#if BUILDFLAG(USE_V4L2_CODEC)
  return true;
#else
  return false;
#endif
}

inline bool UseLibV4L2() {
#if BUILDFLAG(USE_LIBV4L2)
  return true;
#else
  return false;
#endif
}

inline bool IsLibVAVersion2() {
#if BUILDFLAG(USE_VAAPI) && VA_MAJOR_VERSION == 1
  return true;
#else
  return false;
#endif
}

constexpr int dlopen_flag = RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE;

void AddV4L2GpuWhitelist(
    std::vector<BrokerFilePermission>* permissions,
    const service_manager::SandboxSeccompBPF::Options& options) {
  if (options.accelerated_video_decode_enabled) {
    // Device nodes for V4L2 video decode accelerator drivers.
    static const base::FilePath::CharType kDevicePath[] =
        FILE_PATH_LITERAL("/dev/");
    static const base::FilePath::CharType kVideoDecPattern[] = "video-dec[0-9]";
    base::FileEnumerator enumerator(base::FilePath(kDevicePath), false,
                                    base::FileEnumerator::FILES,
                                    base::FilePath(kVideoDecPattern).value());
    for (base::FilePath name = enumerator.Next(); !name.empty();
         name = enumerator.Next())
      permissions->push_back(BrokerFilePermission::ReadWrite(name.value()));
  }

  // Device node for V4L2 video encode accelerator drivers.
  static const char kDevVideoEncPath[] = "/dev/video-enc";
  permissions->push_back(BrokerFilePermission::ReadWrite(kDevVideoEncPath));

  // Device node for V4L2 JPEG decode accelerator drivers.
  static const char kDevJpegDecPath[] = "/dev/jpeg-dec";
  permissions->push_back(BrokerFilePermission::ReadWrite(kDevJpegDecPath));
}

void AddArmMaliGpuWhitelist(std::vector<BrokerFilePermission>* permissions) {
  // Device file needed by the ARM GPU userspace.
  static const char kMali0Path[] = "/dev/mali0";

  // Image processor used on ARM platforms.
  static const char kDevImageProc0Path[] = "/dev/image-proc0";

  permissions->push_back(BrokerFilePermission::ReadWrite(kMali0Path));
  permissions->push_back(BrokerFilePermission::ReadWrite(kDevImageProc0Path));
}

void AddAmdGpuWhitelist(std::vector<BrokerFilePermission>* permissions) {
  static const char* const kReadOnlyList[] = {"/etc/ld.so.cache",
                                              "/usr/lib64/libEGL.so.1",
                                              "/usr/lib64/libGLESv2.so.2"};
  for (const char* item : kReadOnlyList)
    permissions->push_back(BrokerFilePermission::ReadOnly(item));

  static const char* const kReadWriteList[] = {
      "/dev/dri",
      "/dev/dri/card0",
      "/dev/dri/controlD64",
      "/dev/dri/renderD128",
      "/sys/class/drm/card0/device/config",
      "/sys/class/drm/controlD64/device/config",
      "/sys/class/drm/renderD128/device/config",
      "/usr/share/libdrm/amdgpu.ids"};
  for (const char* item : kReadWriteList)
    permissions->push_back(BrokerFilePermission::ReadWrite(item));

  static const char kCharDevices[] = "/sys/dev/char/";
  permissions->push_back(BrokerFilePermission::ReadOnlyRecursive(kCharDevices));
}

void AddArmGpuWhitelist(std::vector<BrokerFilePermission>* permissions) {
  // On ARM we're enabling the sandbox before the X connection is made,
  // so we need to allow access to |.Xauthority|.
  static const char kXAuthorityPath[] = "/home/chronos/.Xauthority";
  static const char kLdSoCache[] = "/etc/ld.so.cache";

  // Files needed by the ARM GPU userspace.
  static const char kLibGlesPath[] = "/usr/lib/libGLESv2.so.2";
  static const char kLibEglPath[] = "/usr/lib/libEGL.so.1";

  permissions->push_back(BrokerFilePermission::ReadOnly(kXAuthorityPath));
  permissions->push_back(BrokerFilePermission::ReadOnly(kLdSoCache));
  permissions->push_back(BrokerFilePermission::ReadOnly(kLibGlesPath));
  permissions->push_back(BrokerFilePermission::ReadOnly(kLibEglPath));

  AddArmMaliGpuWhitelist(permissions);
}

void AddStandardGpuWhiteList(std::vector<BrokerFilePermission>* permissions) {
  static const char kDriCardBasePath[] = "/dev/dri/card";
  static const char kNvidiaCtlPath[] = "/dev/nvidiactl";
  static const char kNvidiaDeviceBasePath[] = "/dev/nvidia";
  static const char kNvidiaParamsPath[] = "/proc/driver/nvidia/params";
  static const char kDevShm[] = "/dev/shm/";

  // For shared memory.
  permissions->push_back(
      BrokerFilePermission::ReadWriteCreateTemporaryRecursive(kDevShm));

  // For DRI cards.
  for (int i = 0; i <= 9; ++i) {
    permissions->push_back(BrokerFilePermission::ReadWrite(
        base::StringPrintf("%s%d", kDriCardBasePath, i)));
  }

  // For Nvidia GLX driver.
  permissions->push_back(BrokerFilePermission::ReadWrite(kNvidiaCtlPath));
  for (int i = 0; i < 10; ++i) {
    permissions->push_back(BrokerFilePermission::ReadWrite(
        base::StringPrintf("%s%d", kNvidiaDeviceBasePath, i)));
  }
  permissions->push_back(BrokerFilePermission::ReadOnly(kNvidiaParamsPath));
}

std::vector<BrokerFilePermission> FilePermissionsForGpu(
    const service_manager::SandboxSeccompBPF::Options& options) {
  std::vector<BrokerFilePermission> permissions;

  // All GPU process policies need this file brokered out.
  static const char kDriRcPath[] = "/etc/drirc";
  permissions.push_back(BrokerFilePermission::ReadOnly(kDriRcPath));

  if (IsChromeOS()) {
    if (UseV4L2Codec())
      AddV4L2GpuWhitelist(&permissions, options);
    if (IsArchitectureArm()) {
      AddArmGpuWhitelist(&permissions);
      return permissions;
    }
    if (options.use_amd_specific_policies) {
      AddAmdGpuWhitelist(&permissions);
      return permissions;
    }
  }
  AddStandardGpuWhiteList(&permissions);
  return permissions;
}

void LoadArmGpuLibraries() {
  // Preload the Mali library.
  dlopen("/usr/lib/libmali.so", dlopen_flag);

  // Preload the Tegra V4L2 (video decode acceleration) library.
  dlopen("/usr/lib/libtegrav4l2.so", dlopen_flag);
}

bool LoadAmdGpuLibraries() {
  // Preload the amdgpu-dependent libraries.
  if (nullptr == dlopen("libglapi.so", dlopen_flag)) {
    LOG(ERROR) << "dlopen(libglapi.so) failed with error: " << dlerror();
    return false;
  }

  const char* radeonsi_lib = "/usr/lib64/dri/radeonsi_dri.so";
#if defined(DRI_DRIVER_DIR)
  radeonsi_lib = DRI_DRIVER_DIR "/radeonsi_dri.so";
#endif
  if (nullptr == dlopen(radeonsi_lib, dlopen_flag)) {
    LOG(ERROR) << "dlopen(radeonsi_dri.so) failed with error: " << dlerror();
    return false;
  }
  return true;
}

void LoadV4L2Libraries() {
  if (UseLibV4L2()) {
    dlopen("/usr/lib/libv4l2.so", dlopen_flag);

    // This is a device-specific encoder plugin.
    dlopen("/usr/lib/libv4l/plugins/libv4l-encplugin.so", dlopen_flag);
  }
}

void LoadStandardLibraries(
    const service_manager::SandboxSeccompBPF::Options& options) {
  if (IsArchitectureX86_64() || IsArchitectureI386()) {
    // Accelerated video dlopen()'s some shared objects
    // inside the sandbox, so preload them now.
    if (options.vaapi_accelerated_video_encode_enabled ||
        options.accelerated_video_decode_enabled) {
      if (IsLibVAVersion2()) {
        if (IsArchitectureX86_64()) {
          dlopen("/usr/lib64/va/drivers/i965_drv_video.so", dlopen_flag);
          dlopen("/usr/lib64/va/drivers/hybrid_drv_video.so", dlopen_flag);
        } else if (IsArchitectureI386()) {
          dlopen("/usr/lib/va/drivers/i965_drv_video.so", dlopen_flag);
        }
        dlopen("libva.so.2", dlopen_flag);
#if defined(USE_OZONE)
        dlopen("libva-drm.so.2", dlopen_flag);
#endif
      } else {
        // If we are linked against libva 1, we have two cases to handle:
        // - the sysroot includes both libva 1 and 2, in which case the drivers
        //   are in /usr/lib{64}/va1/
        // - the sysroot only includes libva 1, in which case the drivers are
        //   are in /usr/lib{64}/va/
        // This is ugly, but temporary until all builds have switched to libva 2.
        if (IsArchitectureX86_64()) {
          if (!dlopen("/usr/lib64/va1/drivers/i965_drv_video.so", dlopen_flag))
            dlopen("/usr/lib64/va/drivers/i965_drv_video.so", dlopen_flag);
          if (!dlopen("/usr/lib64/va1/drivers/hybrid_drv_video.so", dlopen_flag))
            dlopen("/usr/lib64/va/drivers/hybrid_drv_video.so", dlopen_flag);
        } else if (IsArchitectureI386()) {
          if (!dlopen("/usr/lib/va1/drivers/i965_drv_video.so", dlopen_flag))
            dlopen("/usr/lib/va/drivers/i965_drv_video.so", dlopen_flag);
        }
        dlopen("libva.so.1", dlopen_flag);
#if defined(USE_OZONE)
        dlopen("libva-drm.so.1", dlopen_flag);
#elif defined(USE_X11)
        dlopen("libva-x11.so.1", dlopen_flag);
#endif
      }
    }
  }
}

bool LoadLibrariesForGpu(
    const service_manager::SandboxSeccompBPF::Options& options) {
  if (IsChromeOS()) {
    if (UseV4L2Codec())
      LoadV4L2Libraries();
    if (IsArchitectureArm()) {
      LoadArmGpuLibraries();
      return true;
    }
    if (options.use_amd_specific_policies)
      return LoadAmdGpuLibraries();
  }
  LoadStandardLibraries(options);
  return true;
}

bool BrokerProcessPreSandboxHook(
    service_manager::BPFBasePolicy* policy,
    service_manager::SandboxLinux::Options options) {
  // Oddly enough, we call back into gpu to invoke this service manager
  // method, since it is part of the embedder component, and the service
  // mananger's sandbox component is a lower layer that can't depend on it.
  service_manager::SetProcessTitleFromCommandLine(nullptr);
  return true;
}

}  // namespace

bool GpuProcessPreSandboxHook(service_manager::BPFBasePolicy* policy,
                              service_manager::SandboxLinux::Options options) {
  auto* instance = service_manager::SandboxLinux::GetInstance();
  instance->StartBrokerProcess(policy, FilePermissionsForGpu(options),
                               base::BindOnce(BrokerProcessPreSandboxHook),
                               options);

  if (!LoadLibrariesForGpu(options))
    return false;

  // TODO(tsepez): enable namspace sandbox here once crashes are understood.

  errno = 0;
  return true;
}

}  // namespace content
