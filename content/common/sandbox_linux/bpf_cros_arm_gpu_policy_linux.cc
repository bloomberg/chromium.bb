// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/bpf_cros_arm_gpu_policy_linux.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "content/common/sandbox_linux/sandbox_bpf_base_policy_linux.h"
#include "content/common/sandbox_linux/sandbox_seccomp_bpf_linux.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/linux_syscalls.h"

using sandbox::ErrorCode;
using sandbox::SandboxBPF;
using sandbox::SyscallSets;

namespace content {

namespace {

inline bool IsChromeOS() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureArm() {
#if defined(__arm__)
  return true;
#else
  return false;
#endif
}

void AddArmMaliGpuWhitelist(std::vector<std::string>* read_whitelist,
                            std::vector<std::string>* write_whitelist) {
  // Device file needed by the ARM GPU userspace.
  static const char kMali0Path[] = "/dev/mali0";

  // Devices needed for video decode acceleration on ARM.
  static const char kDevMfcDecPath[] = "/dev/mfc-dec";
  static const char kDevGsc1Path[] = "/dev/gsc1";

  // Devices needed for video encode acceleration on ARM.
  static const char kDevMfcEncPath[] = "/dev/mfc-enc";

  read_whitelist->push_back(kMali0Path);
  read_whitelist->push_back(kDevMfcDecPath);
  read_whitelist->push_back(kDevGsc1Path);
  read_whitelist->push_back(kDevMfcEncPath);

  write_whitelist->push_back(kMali0Path);
  write_whitelist->push_back(kDevMfcDecPath);
  write_whitelist->push_back(kDevGsc1Path);
  write_whitelist->push_back(kDevMfcEncPath);
}

void AddArmTegraGpuWhitelist(std::vector<std::string>* read_whitelist,
                             std::vector<std::string>* write_whitelist) {
  // Device files needed by the Tegra GPU userspace.
  static const char kDevNvhostCtrlPath[] = "/dev/nvhost-ctrl";
  static const char kDevNvhostIspPath[] = "/dev/nvhost-isp";
  static const char kDevNvhostViPath[] = "/dev/nvhost-vi";
  static const char kDevNvmapPath[] = "/dev/nvmap";
  static const char kDevNvhostGpuPath[] = "/dev/nvhost-gpu";
  static const char kDevNvhostAsGpuPath[] = "/dev/nvhost-as-gpu";
  static const char kDevNvhostCtrlGpuPath[] = "/dev/nvhost-ctrl-gpu";
  static const char kSysDevicesSocIDPath[] = "/sys/devices/soc0/soc_id";
  static const char kSysDevicesSocRevPath[] = "/sys/devices/soc0/revision";
  // TODO(davidung): remove these device nodes before nyan launch.

  read_whitelist->push_back(kDevNvhostCtrlPath);
  read_whitelist->push_back(kDevNvhostIspPath);
  read_whitelist->push_back(kDevNvhostViPath);
  read_whitelist->push_back(kDevNvmapPath);
  read_whitelist->push_back(kDevNvhostGpuPath);
  read_whitelist->push_back(kDevNvhostAsGpuPath);
  read_whitelist->push_back(kDevNvhostCtrlGpuPath);
  read_whitelist->push_back(kSysDevicesSocIDPath);
  read_whitelist->push_back(kSysDevicesSocRevPath);

  write_whitelist->push_back(kDevNvhostCtrlPath);
  write_whitelist->push_back(kDevNvhostIspPath);
  write_whitelist->push_back(kDevNvhostViPath);
  write_whitelist->push_back(kDevNvmapPath);
  write_whitelist->push_back(kDevNvhostGpuPath);
  write_whitelist->push_back(kDevNvhostAsGpuPath);
  write_whitelist->push_back(kDevNvhostCtrlGpuPath);
}

void AddArmGpuWhitelist(std::vector<std::string>* read_whitelist,
                        std::vector<std::string>* write_whitelist) {
  // On ARM we're enabling the sandbox before the X connection is made,
  // so we need to allow access to |.Xauthority|.
  static const char kXAuthorityPath[] = "/home/chronos/.Xauthority";
  static const char kLdSoCache[] = "/etc/ld.so.cache";

  // Files needed by the ARM GPU userspace.
  static const char kLibGlesPath[] = "/usr/lib/libGLESv2.so.2";
  static const char kLibEglPath[] = "/usr/lib/libEGL.so.1";

  read_whitelist->push_back(kXAuthorityPath);
  read_whitelist->push_back(kLdSoCache);
  read_whitelist->push_back(kLibGlesPath);
  read_whitelist->push_back(kLibEglPath);

  AddArmMaliGpuWhitelist(read_whitelist, write_whitelist);
  AddArmTegraGpuWhitelist(read_whitelist, write_whitelist);
}

class CrosArmGpuBrokerProcessPolicy : public CrosArmGpuProcessPolicy {
 public:
  CrosArmGpuBrokerProcessPolicy() : CrosArmGpuProcessPolicy(false) {}
  virtual ~CrosArmGpuBrokerProcessPolicy() {}

  virtual ErrorCode EvaluateSyscall(SandboxBPF* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosArmGpuBrokerProcessPolicy);
};

// A GPU broker policy is the same as a GPU policy with open and
// openat allowed.
ErrorCode CrosArmGpuBrokerProcessPolicy::EvaluateSyscall(SandboxBPF* sandbox,
    int sysno) const {
  switch (sysno) {
    case __NR_access:
    case __NR_open:
    case __NR_openat:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    default:
      return CrosArmGpuProcessPolicy::EvaluateSyscall(sandbox, sysno);
  }
}

bool EnableArmGpuBrokerPolicyCallback() {
  return SandboxSeccompBPF::StartSandboxWithExternalPolicy(
      scoped_ptr<sandbox::SandboxBPFPolicy>(new CrosArmGpuBrokerProcessPolicy));
}

}  // namespace

CrosArmGpuProcessPolicy::CrosArmGpuProcessPolicy(bool allow_shmat)
    : allow_shmat_(allow_shmat) {}

CrosArmGpuProcessPolicy::~CrosArmGpuProcessPolicy() {}

ErrorCode CrosArmGpuProcessPolicy::EvaluateSyscall(SandboxBPF* sandbox,
                                                   int sysno) const {
#if defined(__arm__)
  if (allow_shmat_ && sysno == __NR_shmat)
    return ErrorCode(ErrorCode::ERR_ALLOWED);
#endif  // defined(__arm__)

  switch (sysno) {
#if defined(__arm__)
    // ARM GPU sandbox is started earlier so we need to allow networking
    // in the sandbox.
    case __NR_connect:
    case __NR_getpeername:
    case __NR_getsockname:
    case __NR_sysinfo:
    case __NR_uname:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    // Allow only AF_UNIX for |domain|.
    case __NR_socket:
    case __NR_socketpair:
      return sandbox->Cond(0, ErrorCode::TP_32BIT,
                           ErrorCode::OP_EQUAL, AF_UNIX,
                           ErrorCode(ErrorCode::ERR_ALLOWED),
                           ErrorCode(EPERM));
#endif  // defined(__arm__)
    default:
      if (SyscallSets::IsAdvancedScheduler(sysno))
        return ErrorCode(ErrorCode::ERR_ALLOWED);

      // Default to the generic GPU policy.
      return GpuProcessPolicy::EvaluateSyscall(sandbox, sysno);
  }
}

bool CrosArmGpuProcessPolicy::PreSandboxHook() {
  DCHECK(IsChromeOS() && IsArchitectureArm());
  // Create a new broker process.
  DCHECK(!broker_process());

  std::vector<std::string> read_whitelist_extra;
  std::vector<std::string> write_whitelist_extra;
  // Add ARM-specific files to whitelist in the broker.

  AddArmGpuWhitelist(&read_whitelist_extra, &write_whitelist_extra);
  InitGpuBrokerProcess(EnableArmGpuBrokerPolicyCallback,
                       read_whitelist_extra,
                       write_whitelist_extra);

  const int dlopen_flag = RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE;

  // Preload the Mali library.
  dlopen("/usr/lib/libmali.so", dlopen_flag);

  // Preload the Tegra libraries.
  dlopen("/usr/lib/libnvrm.so", dlopen_flag);
  dlopen("/usr/lib/libnvrm_graphics.so", dlopen_flag);
  dlopen("/usr/lib/libnvidia-glsi.so", dlopen_flag);
  dlopen("/usr/lib/libnvidia-rmapi-tegra.so", dlopen_flag);
  dlopen("/usr/lib/libnvidia-eglcore.so", dlopen_flag);
  // TODO(davidung): remove these libraries before nyan launch.

  return true;
}

}  // namespace content
