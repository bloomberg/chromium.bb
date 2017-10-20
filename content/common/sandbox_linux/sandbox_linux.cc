// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/sandbox_linux.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/sandbox_linux/sandbox_seccomp_bpf_linux.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_linux.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/namespace_sandbox.h"
#include "sandbox/linux/services/proc_util.h"
#include "sandbox/linux/services/resource_limits.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/services/yama.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"
#include "services/service_manager/sandbox/sandbox_type.h"

#if defined(ANY_OF_AMTLU_SANITIZER)
#include <sanitizer/common_interface_defs.h>
#endif

using sandbox::Yama;

namespace {

struct FDCloser {
  inline void operator()(int* fd) const {
    DCHECK(fd);
    PCHECK(0 == IGNORE_EINTR(close(*fd)));
    *fd = -1;
  }
};

void LogSandboxStarted(const std::string& sandbox_name) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  const std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  const std::string activated_sandbox =
      "Activated " + sandbox_name + " sandbox for process type: " +
      process_type + ".";
  VLOG(1) << activated_sandbox;
}

bool IsRunningTSAN() {
#if defined(THREAD_SANITIZER)
  return true;
#else
  return false;
#endif
}

// Get a file descriptor to /proc. Either duplicate |proc_fd| or try to open
// it by using the filesystem directly.
// TODO(jln): get rid of this ugly interface.
base::ScopedFD OpenProc(int proc_fd) {
  int ret_proc_fd = -1;
  if (proc_fd >= 0) {
    // If a handle to /proc is available, use it. This allows to bypass file
    // system restrictions.
    ret_proc_fd =
        HANDLE_EINTR(openat(proc_fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  } else {
    // Otherwise, make an attempt to access the file system directly.
    ret_proc_fd = HANDLE_EINTR(
        openat(AT_FDCWD, "/proc/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  }
  DCHECK_LE(0, ret_proc_fd);
  return base::ScopedFD(ret_proc_fd);
}

}  // namespace

namespace content {

LinuxSandbox::LinuxSandbox()
    : proc_fd_(-1),
      seccomp_bpf_started_(false),
      sandbox_status_flags_(kSandboxLinuxInvalid),
      pre_initialized_(false),
      seccomp_bpf_supported_(false),
      seccomp_bpf_with_tsync_supported_(false),
      yama_is_enforcing_(false),
      initialize_sandbox_ran_(false),
      setuid_sandbox_client_(sandbox::SetuidSandboxClient::Create()) {
  if (setuid_sandbox_client_ == NULL) {
    LOG(FATAL) << "Failed to instantiate the setuid sandbox client.";
  }
#if defined(ANY_OF_AMTLU_SANITIZER)
  sanitizer_args_ = base::WrapUnique(new __sanitizer_sandbox_arguments);
  *sanitizer_args_ = {0};
#endif
}

LinuxSandbox::~LinuxSandbox() {
  if (pre_initialized_) {
    CHECK(initialize_sandbox_ran_);
  }
}

LinuxSandbox* LinuxSandbox::GetInstance() {
  LinuxSandbox* instance = base::Singleton<LinuxSandbox>::get();
  CHECK(instance);
  return instance;
}

void LinuxSandbox::PreinitializeSandbox() {
  CHECK(!pre_initialized_);
  seccomp_bpf_supported_ = false;
#if defined(ANY_OF_AMTLU_SANITIZER)
  // Sanitizers need to open some resources before the sandbox is enabled.
  // This should not fork, not launch threads, not open a directory.
  __sanitizer_sandbox_on_notify(sanitizer_args());
  sanitizer_args_.reset();
#endif

  // Open proc_fd_. It would break the security of the setuid sandbox if it was
  // not closed.
  // If LinuxSandbox::PreinitializeSandbox() runs, InitializeSandbox() must run
  // as well.
  proc_fd_ = HANDLE_EINTR(open("/proc", O_DIRECTORY | O_RDONLY | O_CLOEXEC));
  CHECK_GE(proc_fd_, 0);
  // We "pre-warm" the code that detects supports for seccomp BPF.
  if (SandboxSeccompBPF::IsSeccompBPFDesired()) {
    if (!SandboxSeccompBPF::SupportsSandbox()) {
      VLOG(1) << "Lacking support for seccomp-bpf sandbox.";
    } else {
      seccomp_bpf_supported_ = true;
    }

    if (SandboxSeccompBPF::SupportsSandboxWithTsync()) {
      seccomp_bpf_with_tsync_supported_ = true;
    }
  }

  // Yama is a "global", system-level status. We assume it will not regress
  // after startup.
  const int yama_status = Yama::GetStatus();
  yama_is_enforcing_ = (yama_status & Yama::STATUS_PRESENT) &&
                       (yama_status & Yama::STATUS_ENFORCING);
  pre_initialized_ = true;
}

void LinuxSandbox::EngageNamespaceSandbox() {
  CHECK(pre_initialized_);
  // Check being in a new PID namespace created by the namespace sandbox and
  // being the init process.
  CHECK(sandbox::NamespaceSandbox::InNewPidNamespace());
  const pid_t pid = getpid();
  CHECK_EQ(1, pid);

  CHECK(sandbox::Credentials::MoveToNewUserNS());
  // Note: this requires SealSandbox() to be called later in this process to be
  // safe, as this class is keeping a file descriptor to /proc/.
  CHECK(sandbox::Credentials::DropFileSystemAccess(proc_fd_));

  // We do not drop CAP_SYS_ADMIN because we need it to place each child process
  // in its own PID namespace later on.
  std::vector<sandbox::Credentials::Capability> caps;
  caps.push_back(sandbox::Credentials::Capability::SYS_ADMIN);
  CHECK(sandbox::Credentials::SetCapabilities(proc_fd_, caps));
}

std::vector<int> LinuxSandbox::GetFileDescriptorsToClose() {
  std::vector<int> fds;
  if (proc_fd_ >= 0) {
    fds.push_back(proc_fd_);
  }
  return fds;
}

bool LinuxSandbox::InitializeSandbox(
    SandboxSeccompBPF::PreSandboxHook hook,
    const SandboxSeccompBPF::Options& options) {
  return LinuxSandbox::GetInstance()->InitializeSandboxImpl(std::move(hook),
                                                            options);
}

void LinuxSandbox::StopThread(base::Thread* thread) {
  LinuxSandbox::GetInstance()->StopThreadImpl(thread);
}

int LinuxSandbox::GetStatus() {
  if (!pre_initialized_) {
    return 0;
  }
  if (kSandboxLinuxInvalid == sandbox_status_flags_) {
    // Initialize sandbox_status_flags_.
    sandbox_status_flags_ = 0;
    if (setuid_sandbox_client_->IsSandboxed()) {
      sandbox_status_flags_ |= kSandboxLinuxSUID;
      if (setuid_sandbox_client_->IsInNewPIDNamespace())
        sandbox_status_flags_ |= kSandboxLinuxPIDNS;
      if (setuid_sandbox_client_->IsInNewNETNamespace())
        sandbox_status_flags_ |= kSandboxLinuxNetNS;
    } else if (sandbox::NamespaceSandbox::InNewUserNamespace()) {
      sandbox_status_flags_ |= kSandboxLinuxUserNS;
      if (sandbox::NamespaceSandbox::InNewPidNamespace())
        sandbox_status_flags_ |= kSandboxLinuxPIDNS;
      if (sandbox::NamespaceSandbox::InNewNetNamespace())
        sandbox_status_flags_ |= kSandboxLinuxNetNS;
    }

    // We report whether the sandbox will be activated when renderers, workers
    // and PPAPI plugins go through sandbox initialization.
    if (seccomp_bpf_supported()) {
      sandbox_status_flags_ |= kSandboxLinuxSeccompBPF;
    }

    if (seccomp_bpf_with_tsync_supported()) {
      sandbox_status_flags_ |= kSandboxLinuxSeccompTSYNC;
    }

    if (yama_is_enforcing_) {
      sandbox_status_flags_ |= kSandboxLinuxYama;
    }
  }

  return sandbox_status_flags_;
}

// Threads are counted via /proc/self/task. This is a little hairy because of
// PID namespaces and existing sandboxes, so "self" must really be used instead
// of using the pid.
bool LinuxSandbox::IsSingleThreaded() const {
  base::ScopedFD proc_fd(OpenProc(proc_fd_));

  CHECK(proc_fd.is_valid()) << "Could not count threads, the sandbox was not "
                            << "pre-initialized properly.";

  const bool is_single_threaded =
      sandbox::ThreadHelpers::IsSingleThreaded(proc_fd.get());

  return is_single_threaded;
}

bool LinuxSandbox::seccomp_bpf_started() const {
  return seccomp_bpf_started_;
}

sandbox::SetuidSandboxClient*
    LinuxSandbox::setuid_sandbox_client() const {
  return setuid_sandbox_client_.get();
}

// For seccomp-bpf, we use the SandboxSeccompBPF class.
bool LinuxSandbox::StartSeccompBPF(service_manager::SandboxType sandbox_type,
                                   SandboxSeccompBPF::PreSandboxHook hook,
                                   const SandboxSeccompBPF::Options& opts) {
  CHECK(!seccomp_bpf_started_);
  CHECK(pre_initialized_);
  if (!seccomp_bpf_supported())
    return false;

  if (!SandboxSeccompBPF::StartSandbox(sandbox_type, OpenProc(proc_fd_),
                                       std::move(hook), opts)) {
    return false;
  }
  seccomp_bpf_started_ = true;
  LogSandboxStarted("seccomp-bpf");
  return true;
}

bool LinuxSandbox::InitializeSandboxImpl(
    SandboxSeccompBPF::PreSandboxHook hook,
    const SandboxSeccompBPF::Options& options) {
  DCHECK(!initialize_sandbox_ran_);
  initialize_sandbox_ran_ = true;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  service_manager::SandboxType sandbox_type =
      service_manager::SandboxTypeFromCommandLine(*command_line);

  // We need to make absolutely sure that our sandbox is "sealed" before
  // returning.
  // Unretained() since the current object is a Singleton.
  base::ScopedClosureRunner sandbox_sealer(
      base::BindOnce(&LinuxSandbox::SealSandbox, base::Unretained(this)));
  // Make sure that this function enables sandboxes as promised by GetStatus().
  // Unretained() since the current object is a Singleton.
  base::ScopedClosureRunner sandbox_promise_keeper(
      base::BindOnce(&LinuxSandbox::CheckForBrokenPromises,
                     base::Unretained(this), sandbox_type));

  // No matter what, it's always an error to call InitializeSandbox() after
  // threads have been created.
  if (!IsSingleThreaded()) {
    std::string error_message =
        "InitializeSandbox() called with multiple threads in process " +
        process_type + ".";
    // TSAN starts a helper thread, so we don't start the sandbox and don't
    // even report an error about it.
    if (IsRunningTSAN())
      return false;

#if defined(OS_CHROMEOS)
    if (base::SysInfo::IsRunningOnChromeOS() &&
        process_type == switches::kGpuProcess) {
      error_message += " This error can be safely ignored in VMTests.";
    }
#endif

    // The GPU process is allowed to call InitializeSandbox() with threads.
    bool sandbox_failure_fatal = process_type != switches::kGpuProcess;
    // This can be disabled with the '--gpu-sandbox-failures-fatal' flag.
    // Setting the flag with no value or any value different than 'yes' or 'no'
    // is equal to setting '--gpu-sandbox-failures-fatal=yes'.
    if (process_type == switches::kGpuProcess &&
        command_line->HasSwitch(switches::kGpuSandboxFailuresFatal)) {
      const std::string switch_value =
          command_line->GetSwitchValueASCII(switches::kGpuSandboxFailuresFatal);
      sandbox_failure_fatal = switch_value != "no";
    }

    if (sandbox_failure_fatal)
      LOG(FATAL) << error_message;

    LOG(ERROR) << error_message;
    return false;
  }

  // Only one thread is running, pre-initialize if not already done.
  if (!pre_initialized_)
    PreinitializeSandbox();

  DCHECK(!HasOpenDirectories()) <<
      "InitializeSandbox() called after unexpected directories have been " <<
      "opened. This breaks the security of the setuid sandbox.";

  // Attempt to limit the future size of the address space of the process.
  LimitAddressSpace(process_type, options);

  return StartSeccompBPF(sandbox_type, std::move(hook), options);
}

void LinuxSandbox::StopThreadImpl(base::Thread* thread) {
  DCHECK(thread);
  StopThreadAndEnsureNotCounted(thread);
}

bool LinuxSandbox::seccomp_bpf_supported() const {
  CHECK(pre_initialized_);
  return seccomp_bpf_supported_;
}

bool LinuxSandbox::seccomp_bpf_with_tsync_supported() const {
  CHECK(pre_initialized_);
  return seccomp_bpf_with_tsync_supported_;
}

bool LinuxSandbox::LimitAddressSpace(
    const std::string& process_type,
    const SandboxSeccompBPF::Options& options) {
#if !defined(ANY_OF_AMTLU_SANITIZER)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (service_manager::SandboxTypeFromCommandLine(*command_line) ==
      service_manager::SANDBOX_TYPE_NO_SANDBOX) {
    return false;
  }
  // Limit the address space to 4GB.
  // This is in the hope of making some kernel exploits more complex and less
  // reliable. It also limits sprays a little on 64 bits.
  rlim_t address_space_limit = std::numeric_limits<uint32_t>::max();
  rlim_t address_space_limit_max = std::numeric_limits<uint32_t>::max();

  if (sizeof(rlim_t) == 8) {
    // On 64 bits, V8 and possibly others will reserve massive memory ranges and
    // rely on on-demand paging for allocation.  Unfortunately, even
    // MADV_DONTNEED ranges count towards RLIMIT_AS so this is not an option.
    // See crbug.com/169327 for a discussion.
    // On the GPU process, irrespective of V8, we can exhaust a 4GB address
    // space under normal usage, see crbug.com/271119.
    // For now, increase limit to 16GB for renderer, worker, and GPU processes
    // to accomodate.
    if (process_type == switches::kRendererProcess ||
        process_type == switches::kGpuProcess) {
      address_space_limit = 1ULL << 34;
      if (options.has_wasm_trap_handler) {
        // WebAssembly memory objects use a large amount of address space when
        // trap-based bounds checks are enabled. To accomodate this, we allow
        // the address space limit to adjust dynamically up to a certain limit.
        // The limit is currently 4TiB, which should allow enough address space
        // for any reasonable page. See https://crbug.com/750378.
        address_space_limit_max = 1ULL << 42;
      } else {
        // If we are not using trap-based bounds checks, there's no reason to
        // allow the address space limit to grow.
        address_space_limit_max = address_space_limit;
      }
    }
  }

  // By default, add a limit to the VmData memory area that would prevent
  // allocations that can't be index by an int.
  rlim_t new_data_segment_max_size = std::numeric_limits<int>::max();

  if (sizeof(rlim_t) == 8) {
    // On 64 bits, increase the RLIMIT_DATA limit to 8GB.
    // RLIMIT_DATA did not account for mmap()-ed memory until
    // https://github.com/torvalds/linux/commit/84638335900f1995495838fe1bd4870c43ec1f6.
    // When Chrome runs on devices with this patch, it will OOM very easily.
    // See https://crbug.com/752185.
    new_data_segment_max_size = 1ULL << 33;
  }

  bool limited_as = sandbox::ResourceLimits::LowerSoftAndHardLimits(
      RLIMIT_AS, address_space_limit, address_space_limit_max);
  bool limited_data =
      sandbox::ResourceLimits::Lower(RLIMIT_DATA, new_data_segment_max_size);

  // Cache the resource limit before turning on the sandbox.
  base::SysInfo::AmountOfVirtualMemory();

  return limited_as && limited_data;
#else
  base::SysInfo::AmountOfVirtualMemory();
  return false;
#endif  // !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER) &&
        // !defined(THREAD_SANITIZER)
}

bool LinuxSandbox::HasOpenDirectories() const {
  return sandbox::ProcUtil::HasOpenDirectory(proc_fd_);
}

void LinuxSandbox::SealSandbox() {
  if (proc_fd_ >= 0) {
    int ret = IGNORE_EINTR(close(proc_fd_));
    CHECK_EQ(0, ret);
    proc_fd_ = -1;
  }
}

void LinuxSandbox::CheckForBrokenPromises(
    service_manager::SandboxType sandbox_type) {
  if (sandbox_type != service_manager::SANDBOX_TYPE_RENDERER &&
      sandbox_type != service_manager::SANDBOX_TYPE_PPAPI) {
    return;
  }
  // Make sure that any promise made with GetStatus() wasn't broken.
  bool promised_seccomp_bpf_would_start =
      (sandbox_status_flags_ != kSandboxLinuxInvalid) &&
      (GetStatus() & kSandboxLinuxSeccompBPF);
  CHECK(!promised_seccomp_bpf_would_start || seccomp_bpf_started_);
}

void LinuxSandbox::StopThreadAndEnsureNotCounted(base::Thread* thread) const {
  DCHECK(thread);
  base::ScopedFD proc_fd(OpenProc(proc_fd_));
  PCHECK(proc_fd.is_valid());
  CHECK(
      sandbox::ThreadHelpers::StopThreadAndWatchProcFS(proc_fd.get(), thread));
}

}  // namespace content
