// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <asm/unistd.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/net.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ucontext.h>
#include <unistd.h>

#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "content/common/sandbox_linux.h"
#include "content/common/sandbox_seccomp_bpf_linux.h"
#include "content/public/common/content_switches.h"
#include "sandbox/linux/services/broker_process.h"

// These are the only architectures supported for now.
#if defined(__i386__) || defined(__x86_64__) || \
    (defined(__arm__) && (defined(__thumb__) || defined(__ARM_EABI__)))
#define SECCOMP_BPF_SANDBOX
#endif

#if defined(SECCOMP_BPF_SANDBOX)
#include "base/posix/eintr_wrapper.h"
#include "content/common/sandbox_bpf_base_policy_linux.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"
#include "sandbox/linux/services/linux_syscalls.h"

using playground2::arch_seccomp_data;
using playground2::ErrorCode;
using playground2::Sandbox;
using sandbox::BrokerProcess;
using sandbox::BaselinePolicy;
using sandbox::SyscallSets;

namespace content {

namespace {

void StartSandboxWithPolicy(playground2::SandboxBpfPolicy* policy);

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
#if defined(__arm__)
  return true;
#else
  return false;
#endif
}

inline bool IsUsingToolKitGtk() {
#if defined(TOOLKIT_GTK)
  return true;
#else
  return false;
#endif
}

// Policies for the GPU process.
// TODO(jln): move to gpu/

bool IsAcceleratedVideoDecodeEnabled() {
  // Accelerated video decode is currently enabled on Chrome OS,
  // but not on Linux: crbug.com/137247.
  bool is_enabled = IsChromeOS();

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  is_enabled = is_enabled &&
      !command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode);

  return is_enabled;
}

intptr_t GpuSIGSYS_Handler(const struct arch_seccomp_data& args,
                           void* aux_broker_process) {
  RAW_CHECK(aux_broker_process);
  BrokerProcess* broker_process =
      static_cast<BrokerProcess*>(aux_broker_process);
  switch (args.nr) {
    case __NR_access:
      return broker_process->Access(reinterpret_cast<const char*>(args.args[0]),
                                    static_cast<int>(args.args[1]));
    case __NR_open:
      return broker_process->Open(reinterpret_cast<const char*>(args.args[0]),
                                  static_cast<int>(args.args[1]));
    case __NR_openat:
      // Allow using openat() as open().
      if (static_cast<int>(args.args[0]) == AT_FDCWD) {
        return
            broker_process->Open(reinterpret_cast<const char*>(args.args[1]),
                                 static_cast<int>(args.args[2]));
      } else {
        return -EPERM;
      }
    default:
      RAW_CHECK(false);
      return -ENOSYS;
  }
}

class GpuProcessPolicy : public SandboxBpfBasePolicy {
 public:
  explicit GpuProcessPolicy(void* broker_process)
      : broker_process_(broker_process) {}
  virtual ~GpuProcessPolicy() {}

  virtual ErrorCode EvaluateSyscall(Sandbox* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;

 private:
  const void* broker_process_;  // Non-owning pointer.
  DISALLOW_COPY_AND_ASSIGN(GpuProcessPolicy);
};

// Main policy for x86_64/i386. Extended by ArmGpuProcessPolicy.
ErrorCode GpuProcessPolicy::EvaluateSyscall(Sandbox* sandbox, int sysno) const {
  switch (sysno) {
    case __NR_ioctl:
#if defined(__i386__) || defined(__x86_64__)
    // The Nvidia driver uses flags not in the baseline policy
    // (MAP_LOCKED | MAP_EXECUTABLE | MAP_32BIT)
    case __NR_mmap:
#endif
    // We also hit this on the linux_chromeos bot but don't yet know what
    // weird flags were involved.
    case __NR_mprotect:
    case __NR_sched_getaffinity:
    case __NR_sched_setaffinity:
    case __NR_setpriority:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    case __NR_access:
    case __NR_open:
    case __NR_openat:
      return sandbox->Trap(GpuSIGSYS_Handler, broker_process_);
    default:
      if (SyscallSets::IsEventFd(sysno))
        return ErrorCode(ErrorCode::ERR_ALLOWED);

      // Default on the baseline policy.
      return SandboxBpfBasePolicy::EvaluateSyscall(sandbox, sysno);
  }
}

class GpuBrokerProcessPolicy : public GpuProcessPolicy {
 public:
  GpuBrokerProcessPolicy() : GpuProcessPolicy(NULL) {}
  virtual ~GpuBrokerProcessPolicy() {}

  virtual ErrorCode EvaluateSyscall(Sandbox* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuBrokerProcessPolicy);
};

// x86_64/i386.
// A GPU broker policy is the same as a GPU policy with open and
// openat allowed.
ErrorCode GpuBrokerProcessPolicy::EvaluateSyscall(Sandbox* sandbox,
                                                  int sysno) const {
  switch (sysno) {
    case __NR_access:
    case __NR_open:
    case __NR_openat:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    default:
      return GpuProcessPolicy::EvaluateSyscall(sandbox, sysno);
  }
}

class ArmGpuProcessPolicy : public GpuProcessPolicy {
 public:
  explicit ArmGpuProcessPolicy(void* broker_process, bool allow_shmat)
      : GpuProcessPolicy(broker_process), allow_shmat_(allow_shmat) {}
  virtual ~ArmGpuProcessPolicy() {}

  virtual ErrorCode EvaluateSyscall(Sandbox* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;

 private:
  const bool allow_shmat_;  // Allow shmat(2).
  DISALLOW_COPY_AND_ASSIGN(ArmGpuProcessPolicy);
};

// Generic ARM GPU process sandbox, inheriting from GpuProcessPolicy.
ErrorCode ArmGpuProcessPolicy::EvaluateSyscall(Sandbox* sandbox,
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

class ArmGpuBrokerProcessPolicy : public ArmGpuProcessPolicy {
 public:
  ArmGpuBrokerProcessPolicy() : ArmGpuProcessPolicy(NULL, false) {}
  virtual ~ArmGpuBrokerProcessPolicy() {}

  virtual ErrorCode EvaluateSyscall(Sandbox* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArmGpuBrokerProcessPolicy);
};

// A GPU broker policy is the same as a GPU policy with open and
// openat allowed.
ErrorCode ArmGpuBrokerProcessPolicy::EvaluateSyscall(Sandbox* sandbox,
                                                     int sysno) const {
  switch (sysno) {
    case __NR_access:
    case __NR_open:
    case __NR_openat:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    default:
      return ArmGpuProcessPolicy::EvaluateSyscall(sandbox, sysno);
  }
}

// Policy for renderer and worker processes.
// TODO(jln): move to renderer/

class RendererOrWorkerProcessPolicy : public SandboxBpfBasePolicy {
 public:
  RendererOrWorkerProcessPolicy() {}
  virtual ~RendererOrWorkerProcessPolicy() {}

  virtual ErrorCode EvaluateSyscall(Sandbox* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererOrWorkerProcessPolicy);
};

ErrorCode RendererOrWorkerProcessPolicy::EvaluateSyscall(Sandbox* sandbox,
                                                         int sysno) const {
  switch (sysno) {
    case __NR_clone:
      return sandbox::RestrictCloneToThreadsAndEPERMFork(sandbox);
    case __NR_ioctl:
      return sandbox::RestrictIoctl(sandbox);
    case __NR_prctl:
      return sandbox::RestrictPrctl(sandbox);
    // Allow the system calls below.
    case __NR_fdatasync:
    case __NR_fsync:
    case __NR_getpriority:
#if defined(__i386__) || defined(__x86_64__)
    case __NR_getrlimit:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_ugetrlimit:
#endif
    case __NR_mremap:   // See crbug.com/149834.
    case __NR_pread64:
    case __NR_pwrite64:
    case __NR_sched_getaffinity:
    case __NR_sched_get_priority_max:
    case __NR_sched_get_priority_min:
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_setscheduler:
    case __NR_setpriority:
    case __NR_sysinfo:
    case __NR_times:
    case __NR_uname:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    case __NR_prlimit64:
      return ErrorCode(EPERM);  // See crbug.com/160157.
    default:
      if (IsUsingToolKitGtk()) {
#if defined(__x86_64__) || defined(__arm__)
        if (SyscallSets::IsSystemVSharedMemory(sysno))
          return ErrorCode(ErrorCode::ERR_ALLOWED);
#endif
#if defined(__i386__)
        if (SyscallSets::IsSystemVIpc(sysno))
          return ErrorCode(ErrorCode::ERR_ALLOWED);
#endif
      }

      // Default on the content baseline policy.
      return SandboxBpfBasePolicy::EvaluateSyscall(sandbox, sysno);
  }
}

// Policy for PPAPI plugins.
// TODO(jln): move to ppapi_plugin/.
class FlashProcessPolicy : public SandboxBpfBasePolicy {
 public:
  FlashProcessPolicy() {}
  virtual ~FlashProcessPolicy() {}

  virtual ErrorCode EvaluateSyscall(Sandbox* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FlashProcessPolicy);
};

ErrorCode FlashProcessPolicy::EvaluateSyscall(Sandbox* sandbox,
                                              int sysno) const {
  switch (sysno) {
    case __NR_clone:
      return sandbox::RestrictCloneToThreadsAndEPERMFork(sandbox);
    case __NR_pread64:
    case __NR_pwrite64:
    case __NR_sched_get_priority_max:
    case __NR_sched_get_priority_min:
    case __NR_sched_getaffinity:
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_setscheduler:
    case __NR_times:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    case __NR_ioctl:
      return ErrorCode(ENOTTY);  // Flash Access.
    default:
      if (IsUsingToolKitGtk()) {
#if defined(__x86_64__) || defined(__arm__)
        if (SyscallSets::IsSystemVSharedMemory(sysno))
          return ErrorCode(ErrorCode::ERR_ALLOWED);
#endif
#if defined(__i386__)
        if (SyscallSets::IsSystemVIpc(sysno))
          return ErrorCode(ErrorCode::ERR_ALLOWED);
#endif
      }

      // Default on the baseline policy.
      return SandboxBpfBasePolicy::EvaluateSyscall(sandbox, sysno);
  }
}

class BlacklistDebugAndNumaPolicy : public SandboxBpfBasePolicy {
 public:
  BlacklistDebugAndNumaPolicy() {}
  virtual ~BlacklistDebugAndNumaPolicy() {}

  virtual ErrorCode EvaluateSyscall(Sandbox* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlacklistDebugAndNumaPolicy);
};

ErrorCode BlacklistDebugAndNumaPolicy::EvaluateSyscall(Sandbox* sandbox,
                                                       int sysno) const {
  if (!Sandbox::IsValidSyscallNumber(sysno)) {
    // TODO(jln) we should not have to do that in a trivial policy.
    return ErrorCode(ENOSYS);
  }
  if (SyscallSets::IsDebug(sysno) || SyscallSets::IsNuma(sysno))
    return sandbox->Trap(sandbox::CrashSIGSYS_Handler, NULL);

  return ErrorCode(ErrorCode::ERR_ALLOWED);
}

class AllowAllPolicy : public SandboxBpfBasePolicy {
 public:
  AllowAllPolicy() {}
  virtual ~AllowAllPolicy() {}

  virtual ErrorCode EvaluateSyscall(Sandbox* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AllowAllPolicy);
};

// Allow all syscalls.
// This will still deny x32 or IA32 calls in 64 bits mode or
// 64 bits system calls in compatibility mode.
ErrorCode AllowAllPolicy::EvaluateSyscall(Sandbox*, int sysno) const {
  if (!Sandbox::IsValidSyscallNumber(sysno)) {
    // TODO(jln) we should not have to do that in a trivial policy.
    return ErrorCode(ENOSYS);
  } else {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

// If a BPF policy is engaged for |process_type|, run a few sanity checks.
void RunSandboxSanityChecks(const std::string& process_type) {
  if (process_type == switches::kRendererProcess ||
      process_type == switches::kWorkerProcess ||
      process_type == switches::kGpuProcess ||
      process_type == switches::kPpapiPluginProcess) {
    int syscall_ret;
    errno = 0;

    // Without the sandbox, this would EBADF.
    syscall_ret = fchmod(-1, 07777);
    CHECK_EQ(-1, syscall_ret);
    CHECK_EQ(EPERM, errno);

    // Run most of the sanity checks only in DEBUG mode to avoid a perf.
    // impact.
#if !defined(NDEBUG)
    // open() must be restricted.
    syscall_ret = open("/etc/passwd", O_RDONLY);
    CHECK_EQ(-1, syscall_ret);
    CHECK_EQ(SandboxBpfBasePolicy::GetFSDeniedErrno(), errno);

    // We should never allow the creation of netlink sockets.
    syscall_ret = socket(AF_NETLINK, SOCK_DGRAM, 0);
    CHECK_EQ(-1, syscall_ret);
    CHECK_EQ(EPERM, errno);
#endif  // !defined(NDEBUG)
  }
}

bool EnableGpuBrokerPolicyCallback() {
  StartSandboxWithPolicy(new GpuBrokerProcessPolicy);
  return true;
}

bool EnableArmGpuBrokerPolicyCallback() {
  StartSandboxWithPolicy(new ArmGpuBrokerProcessPolicy);
  return true;
}

// Files needed by the ARM GPU userspace.
static const char kLibGlesPath[] = "/usr/lib/libGLESv2.so.2";
static const char kLibEglPath[] = "/usr/lib/libEGL.so.1";

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
  static const char kDevNvhostGr2dPath[] = "/dev/nvhost-gr2d";
  static const char kDevNvhostGr3dPath[] = "/dev/nvhost-gr3d";
  static const char kDevNvhostIspPath[] = "/dev/nvhost-isp";
  static const char kDevNvhostViPath[] = "/dev/nvhost-vi";
  static const char kDevNvmapPath[] = "/dev/nvmap";
  static const char kDevTegraSemaPath[] = "/dev/tegra_sema";

  read_whitelist->push_back(kDevNvhostCtrlPath);
  read_whitelist->push_back(kDevNvhostGr2dPath);
  read_whitelist->push_back(kDevNvhostGr3dPath);
  read_whitelist->push_back(kDevNvhostIspPath);
  read_whitelist->push_back(kDevNvhostViPath);
  read_whitelist->push_back(kDevNvmapPath);
  read_whitelist->push_back(kDevTegraSemaPath);

  write_whitelist->push_back(kDevNvhostCtrlPath);
  write_whitelist->push_back(kDevNvhostGr2dPath);
  write_whitelist->push_back(kDevNvhostGr3dPath);
  write_whitelist->push_back(kDevNvhostIspPath);
  write_whitelist->push_back(kDevNvhostViPath);
  write_whitelist->push_back(kDevNvmapPath);
  write_whitelist->push_back(kDevTegraSemaPath);
}

void AddArmGpuWhitelist(std::vector<std::string>* read_whitelist,
                        std::vector<std::string>* write_whitelist) {
  // On ARM we're enabling the sandbox before the X connection is made,
  // so we need to allow access to |.Xauthority|.
  static const char kXAuthorityPath[] = "/home/chronos/.Xauthority";
  static const char kLdSoCache[] = "/etc/ld.so.cache";

  read_whitelist->push_back(kXAuthorityPath);
  read_whitelist->push_back(kLdSoCache);
  read_whitelist->push_back(kLibGlesPath);
  read_whitelist->push_back(kLibEglPath);

  AddArmMaliGpuWhitelist(read_whitelist, write_whitelist);
  AddArmTegraGpuWhitelist(read_whitelist, write_whitelist);
}

// Start a broker process to handle open() inside the sandbox.
void InitGpuBrokerProcess(bool for_chromeos_arm,
                          BrokerProcess** broker_process) {
  static const char kDriRcPath[] = "/etc/drirc";
  static const char kDriCard0Path[] = "/dev/dri/card0";

  CHECK(broker_process);
  CHECK(*broker_process == NULL);

  bool (*sandbox_callback)(void) = NULL;

  // All GPU process policies need these files brokered out.
  std::vector<std::string> read_whitelist;
  read_whitelist.push_back(kDriCard0Path);
  read_whitelist.push_back(kDriRcPath);

  std::vector<std::string> write_whitelist;
  write_whitelist.push_back(kDriCard0Path);

  if (for_chromeos_arm) {
    // We shouldn't be using this policy on non-ARM architectures.
    DCHECK(IsArchitectureArm());
    AddArmGpuWhitelist(&read_whitelist, &write_whitelist);
    sandbox_callback = EnableArmGpuBrokerPolicyCallback;
  } else {
    sandbox_callback = EnableGpuBrokerPolicyCallback;
  }

  *broker_process = new BrokerProcess(SandboxBpfBasePolicy::GetFSDeniedErrno(),
                                      read_whitelist,
                                      write_whitelist);
  // Initialize the broker process and give it a sandbox callback.
  CHECK((*broker_process)->Init(sandbox_callback));
}

// Warms up/preloads resources needed by the policies.
// Eventually start a broker process and return it in broker_process.
void WarmupPolicy(bool chromeos_arm_gpu,
                  BrokerProcess** broker_process) {
  if (!chromeos_arm_gpu) {
    // Create a new broker process.
    InitGpuBrokerProcess(false /* not for ChromeOS ARM */, broker_process);

    if (IsArchitectureX86_64() || IsArchitectureI386()) {
      // Accelerated video decode dlopen()'s some shared objects
      // inside the sandbox, so preload them now.
      if (IsAcceleratedVideoDecodeEnabled()) {
        const char* I965DrvVideoPath = NULL;

        if (IsArchitectureX86_64()) {
          I965DrvVideoPath = "/usr/lib64/va/drivers/i965_drv_video.so";
        } else if (IsArchitectureI386()) {
          I965DrvVideoPath = "/usr/lib/va/drivers/i965_drv_video.so";
        }

        dlopen(I965DrvVideoPath, RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
        dlopen("libva.so.1", RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
        dlopen("libva-x11.so.1", RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
      }
    }
  } else {
    // ChromeOS ARM GPU policy.
    // Create a new broker process.
    InitGpuBrokerProcess(true /* for ChromeOS ARM */, broker_process);

    // Preload the Mali library.
    dlopen("/usr/lib/libmali.so", RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);

    // Preload the Tegra libraries.
    dlopen("/usr/lib/libnvrm.so", RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
    dlopen("/usr/lib/libnvrm_graphics.so", RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
    dlopen("/usr/lib/libnvos.so", RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
    dlopen("/usr/lib/libnvddk_2d.so", RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
    dlopen("/usr/lib/libardrv_dynamic.so", RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
    dlopen("/usr/lib/libnvwsi.so", RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
    dlopen("/usr/lib/libnvglsi.so", RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
    dlopen("/usr/lib/libcgdrv.so", RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
  }
}

void StartGpuProcessSandbox(const CommandLine& command_line,
                            const std::string& process_type) {
  bool chromeos_arm_gpu = false;
  bool allow_sysv_shm = false;

  if (process_type == switches::kGpuProcess) {
    // On Chrome OS ARM, we need a specific GPU process policy.
    if (IsChromeOS() && IsArchitectureArm()) {
      chromeos_arm_gpu = true;
      if (command_line.HasSwitch(switches::kGpuSandboxAllowSysVShm)) {
        allow_sysv_shm = true;
      }
    }
  }

  // This should never be destroyed, as after the sandbox is started it is
  // vital to the process. Ownership is transfered to the policies and then to
  // the BPF sandbox which will keep it around to service SIGSYS traps from the
  // kernel.
  BrokerProcess* broker_process = NULL;
  // Warm up resources needed by the policy we're about to enable and
  // eventually start a broker process.
  WarmupPolicy(chromeos_arm_gpu, &broker_process);

  scoped_ptr<SandboxBpfBasePolicy> gpu_policy;
  if (chromeos_arm_gpu) {
    gpu_policy.reset(new ArmGpuProcessPolicy(broker_process, allow_sysv_shm));
  } else {
    gpu_policy.reset(new GpuProcessPolicy(broker_process));
  }
  StartSandboxWithPolicy(gpu_policy.release());
}

// This function takes ownership of |policy|.
void StartSandboxWithPolicy(playground2::SandboxBpfPolicy* policy) {
  // Starting the sandbox is a one-way operation. The kernel doesn't allow
  // us to unload a sandbox policy after it has been started. Nonetheless,
  // in order to make the use of the "Sandbox" object easier, we allow for
  // the object to be destroyed after the sandbox has been started. Note that
  // doing so does not stop the sandbox.
  Sandbox sandbox;
  sandbox.SetSandboxPolicy(policy);
  sandbox.StartSandbox();
}

void StartNonGpuSandbox(const std::string& process_type) {
  scoped_ptr<SandboxBpfBasePolicy> policy;

  if (process_type == switches::kRendererProcess ||
      process_type == switches::kWorkerProcess) {
    policy.reset(new RendererOrWorkerProcessPolicy);
  } else if (process_type == switches::kPpapiPluginProcess) {
    policy.reset(new FlashProcessPolicy);
  } else if (process_type == switches::kUtilityProcess) {
    policy.reset(new BlacklistDebugAndNumaPolicy);
  } else {
    NOTREACHED();
    policy.reset(new AllowAllPolicy);
  }

  StartSandboxWithPolicy(policy.release());
}

// Initialize the seccomp-bpf sandbox.
bool StartBpfSandbox(const CommandLine& command_line,
                     const std::string& process_type) {

  if (process_type == switches::kGpuProcess) {
    StartGpuProcessSandbox(command_line, process_type);
  } else {
    StartNonGpuSandbox(process_type);
  }

  RunSandboxSanityChecks(process_type);
  return true;
}

}  // namespace

#endif  // SECCOMP_BPF_SANDBOX

// Is seccomp BPF globally enabled?
bool SandboxSeccompBpf::IsSeccompBpfDesired() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kNoSandbox) &&
      !command_line.HasSwitch(switches::kDisableSeccompFilterSandbox)) {
    return true;
  } else {
    return false;
  }
}

bool SandboxSeccompBpf::ShouldEnableSeccompBpf(
    const std::string& process_type) {
#if defined(SECCOMP_BPF_SANDBOX)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (process_type == switches::kGpuProcess)
    return !command_line.HasSwitch(switches::kDisableGpuSandbox);

  return true;
#endif  // SECCOMP_BPF_SANDBOX
  return false;
}

bool SandboxSeccompBpf::SupportsSandbox() {
#if defined(SECCOMP_BPF_SANDBOX)
  // TODO(jln): pass the saved proc_fd_ from the LinuxSandbox singleton
  // here.
  Sandbox::SandboxStatus bpf_sandbox_status =
      Sandbox::SupportsSeccompSandbox(-1);
  // Kernel support is what we are interested in here. Other status
  // such as STATUS_UNAVAILABLE (has threads) still indicate kernel support.
  // We make this a negative check, since if there is a bug, we would rather
  // "fail closed" (expect a sandbox to be available and try to start it).
  if (bpf_sandbox_status != Sandbox::STATUS_UNSUPPORTED) {
    return true;
  }
#endif
  return false;
}

bool SandboxSeccompBpf::StartSandbox(const std::string& process_type) {
#if defined(SECCOMP_BPF_SANDBOX)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (IsSeccompBpfDesired() &&  // Global switches policy.
      ShouldEnableSeccompBpf(process_type) &&  // Process-specific policy.
      SupportsSandbox()) {
    // If the kernel supports the sandbox, and if the command line says we
    // should enable it, enable it or die.
    bool started_sandbox = StartBpfSandbox(command_line, process_type);
    CHECK(started_sandbox);
    return true;
  }
#endif
  return false;
}

bool SandboxSeccompBpf::StartSandboxWithExternalPolicy(
    scoped_ptr<playground2::SandboxBpfPolicy> policy) {
#if defined(SECCOMP_BPF_SANDBOX)
  if (IsSeccompBpfDesired() && SupportsSandbox()) {
    CHECK(policy);
    StartSandboxWithPolicy(policy.release());
    return true;
  }
#endif  // defined(SECCOMP_BPF_SANDBOX)
  return false;
}

scoped_ptr<playground2::SandboxBpfPolicy>
SandboxSeccompBpf::GetBaselinePolicy() {
#if defined(SECCOMP_BPF_SANDBOX)
  return scoped_ptr<playground2::SandboxBpfPolicy>(new BaselinePolicy);
#else
  return scoped_ptr<playground2::SandboxBpfPolicy>();
#endif  // defined(SECCOMP_BPF_SANDBOX)
}

}  // namespace content
