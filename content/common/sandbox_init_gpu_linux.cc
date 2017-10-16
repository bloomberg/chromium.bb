// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_init_gpu_linux.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_file.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "content/common/sandbox_linux/bpf_cros_amd_gpu_policy_linux.h"
#include "content/common/sandbox_linux/bpf_cros_arm_gpu_policy_linux.h"
#include "content/common/sandbox_linux/bpf_gpu_policy_linux.h"
#include "content/common/sandbox_linux/sandbox_seccomp_bpf_linux.h"
#include "content/public/common/content_switches.h"
#include "media/gpu/features.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "services/service_manager/embedder/set_process_title.h"

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
#if defined(__arm__) || defined(__aarch64__)
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

bool IsAcceleratedVaapiVideoEncodeEnabled() {
  bool accelerated_encode_enabled = false;
#if defined(OS_CHROMEOS)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  accelerated_encode_enabled =
      !command_line.HasSwitch(switches::kDisableVaapiAcceleratedVideoEncode);
#endif
  return accelerated_encode_enabled;
}

bool IsAcceleratedVideoDecodeEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return !command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode);
}

void AddV4L2GpuWhitelist(std::vector<BrokerFilePermission>* permissions) {
  if (IsAcceleratedVideoDecodeEnabled()) {
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

void UpdateProcessTypeToGpuBroker() {
  base::CommandLine::StringVector exec =
      base::CommandLine::ForCurrentProcess()->GetArgs();
  base::CommandLine::Reset();
  base::CommandLine::Init(0, NULL);
  base::CommandLine::ForCurrentProcess()->InitFromArgv(exec);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kProcessType, "gpu-broker");

  // Update the process title. The argv was already cached by the call to
  // SetProcessTitleFromCommandLine in content_main_runner.cc, so we can pass
  // NULL here (we don't have the original argv at this point).
  service_manager::SetProcessTitleFromCommandLine(nullptr);
}

bool UpdateProcessTypeAndEnableSandbox(
    std::unique_ptr<Policy> (*broker_sandboxer_allocator)()) {
  DCHECK(broker_sandboxer_allocator);
  UpdateProcessTypeToGpuBroker();
  return SandboxSeccompBPF::StartSandboxWithExternalPolicy(
      broker_sandboxer_allocator(), base::ScopedFD());
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
  static const char* kReadOnlyList[] = {"/etc/ld.so.cache",
                                        "/usr/lib64/libEGL.so.1",
                                        "/usr/lib64/libGLESv2.so.2"};
  int listSize = arraysize(kReadOnlyList);

  for (int i = 0; i < listSize; i++) {
    permissions->push_back(BrokerFilePermission::ReadOnly(kReadOnlyList[i]));
  }

  static const char* kReadWriteList[] = {
      "/dev/dri",
      "/dev/dri/card0",
      "/dev/dri/controlD64",
      "/dev/dri/renderD128",
      "/sys/class/drm/card0/device/config",
      "/sys/class/drm/controlD64/device/config",
      "/sys/class/drm/renderD128/device/config",
      "/usr/share/libdrm/amdgpu.ids"};

  listSize = arraysize(kReadWriteList);

  for (int i = 0; i < listSize; i++) {
    permissions->push_back(BrokerFilePermission::ReadWrite(kReadWriteList[i]));
  }

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

// Start a broker process to handle open() inside the sandbox.
// |broker_sandboxer_allocator| is a function pointer which can allocate a
// suitable sandbox policy for the broker process itself.
// |permissions_extra| is a list of file permissions
// that should be whitelisted by the broker process, in addition to
// the basic ones.
std::unique_ptr<BrokerProcess> InitGpuBrokerProcess(
    std::unique_ptr<Policy> (*broker_sandboxer_allocator)(),
    const std::vector<BrokerFilePermission>& permissions_extra) {
  static const char kDriRcPath[] = "/etc/drirc";
  static const char kDriCardBasePath[] = "/dev/dri/card";
  static const char kNvidiaCtlPath[] = "/dev/nvidiactl";
  static const char kNvidiaDeviceBasePath[] = "/dev/nvidia";
  static const char kNvidiaParamsPath[] = "/proc/driver/nvidia/params";
  static const char kDevShm[] = "/dev/shm/";

  // All GPU process policies need these files brokered out.
  std::vector<BrokerFilePermission> permissions;
  permissions.push_back(BrokerFilePermission::ReadOnly(kDriRcPath));

  if (!IsChromeOS()) {
    // For shared memory.
    permissions.push_back(
        BrokerFilePermission::ReadWriteCreateUnlinkRecursive(kDevShm));
    // For DRI cards.
    for (int i = 0; i <= 9; ++i) {
      permissions.push_back(BrokerFilePermission::ReadWrite(
          base::StringPrintf("%s%d", kDriCardBasePath, i)));
    }
    // For Nvidia GLX driver.
    permissions.push_back(BrokerFilePermission::ReadWrite(kNvidiaCtlPath));
    for (int i = 0; i <= 9; ++i) {
      permissions.push_back(BrokerFilePermission::ReadWrite(
          base::StringPrintf("%s%d", kNvidiaDeviceBasePath, i)));
    }
    permissions.push_back(BrokerFilePermission::ReadOnly(kNvidiaParamsPath));
  } else if (UseV4L2Codec()) {
    AddV4L2GpuWhitelist(&permissions);
    if (UseLibV4L2()) {
      dlopen("/usr/lib/libv4l2.so", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
      // This is a device-specific encoder plugin.
      dlopen("/usr/lib/libv4l/plugins/libv4l-encplugin.so",
             RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
    }
  }

  // Add eventual extra files from permissions_extra.
  for (const auto& perm : permissions_extra) {
    permissions.push_back(perm);
  }

  auto result = std::make_unique<BrokerProcess>(
      SandboxBPFBasePolicy::GetFSDeniedErrno(), permissions);
  // The initialization callback will perform generic initialization and then
  // call broker_sandboxer_callback.
  CHECK(result->Init(base::Bind(&UpdateProcessTypeAndEnableSandbox,
                                broker_sandboxer_allocator)));

  return result;
}

}  // namespace

bool GpuPreSandboxHook(GpuProcessPolicy* policy) {
  // Warm up resources needed by the policy we're about to enable and
  // eventually start a broker process.
  const bool chromeos_arm_gpu = IsChromeOS() && IsArchitectureArm();
  // This policy is for x86 or Desktop.
  DCHECK(!chromeos_arm_gpu);

  // Create a new broker process with no extra files in whitelist.
  policy->set_broker_process(InitGpuBrokerProcess(
      []() -> std::unique_ptr<Policy> {
        return std::make_unique<GpuBrokerProcessPolicy>();
      },
      std::vector<BrokerFilePermission>()));

  if (IsArchitectureX86_64() || IsArchitectureI386()) {
    // Accelerated video dlopen()'s some shared objects
    // inside the sandbox, so preload them now.
    if (IsAcceleratedVaapiVideoEncodeEnabled() ||
        IsAcceleratedVideoDecodeEnabled()) {
      const char* I965DrvVideoPath = NULL;
      const char* I965HybridDrvVideoPath = NULL;

      if (IsArchitectureX86_64()) {
        I965DrvVideoPath = "/usr/lib64/va/drivers/i965_drv_video.so";
        I965HybridDrvVideoPath = "/usr/lib64/va/drivers/hybrid_drv_video.so";
      } else if (IsArchitectureI386()) {
        I965DrvVideoPath = "/usr/lib/va/drivers/i965_drv_video.so";
      }

      dlopen(I965DrvVideoPath, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
      if (I965HybridDrvVideoPath)
        dlopen(I965HybridDrvVideoPath, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
      dlopen("libva.so.1", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
#if defined(USE_OZONE)
      dlopen("libva-drm.so.1", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
#elif defined(USE_X11)
      dlopen("libva-x11.so.1", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
#endif
    }
  }

  return true;
}

bool CrosArmGpuPreSandboxHook(GpuProcessPolicy* policy) {
  DCHECK(IsChromeOS() && IsArchitectureArm());

  // Add ARM-specific files to whitelist in the broker.
  std::vector<BrokerFilePermission> permissions;
  AddArmGpuWhitelist(&permissions);
  policy->set_broker_process(InitGpuBrokerProcess(
      []() -> std::unique_ptr<Policy> {
        return std::make_unique<CrosArmGpuBrokerProcessPolicy>();
      },
      permissions));

  const int dlopen_flag = RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE;

  // Preload the Mali library.
  dlopen("/usr/lib/libmali.so", dlopen_flag);
  // Preload the Tegra V4L2 (video decode acceleration) library.
  dlopen("/usr/lib/libtegrav4l2.so", dlopen_flag);
  // Resetting errno since platform-specific libraries will fail on other
  // platforms.
  errno = 0;

  return true;
}

bool CrosAmdGpuPreSandboxHook(GpuProcessPolicy* policy) {
  DCHECK(IsChromeOS());

  // Add AMD-specific files to whitelist in the broker.
  std::vector<BrokerFilePermission> permissions;
  AddAmdGpuWhitelist(&permissions);

  policy->set_broker_process(InitGpuBrokerProcess(
      []() -> std::unique_ptr<Policy> {
        return std::make_unique<CrosAmdGpuBrokerProcessPolicy>();
      },
      permissions));

  const int dlopen_flag = RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE;

  // Preload the amdgpu-dependent libraries.
  errno = 0;
  if (NULL == dlopen("libglapi.so", dlopen_flag)) {
    LOG(ERROR) << "dlopen(libglapi.so) failed with error: " << dlerror();
    return false;
  }
  if (NULL == dlopen("/usr/lib64/dri/swrast_dri.so", dlopen_flag)) {
    LOG(ERROR) << "dlopen(swrast_dri.so) failed with error: " << dlerror();
    return false;
  }
  if (NULL == dlopen("/usr/lib64/dri/radeonsi_dri.so", dlopen_flag)) {
    LOG(ERROR) << "dlopen(radeonsi_dri.so) failed with error: " << dlerror();
    return false;
  }

  return true;
}

}  // namespace content
