// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/posix/eintr_wrapper.h"
#include "base/time.h"
#include "content/common/sandbox_linux.h"
#include "content/common/sandbox_seccomp_bpf_linux.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_linux.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"

namespace {

void LogSandboxStarted(const std::string& sandbox_name) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  const std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  const std::string activated_sandbox =
      "Activated " + sandbox_name + " sandbox for process type: " +
      process_type + ".";
#if defined(OS_CHROMEOS)
  LOG(WARNING) << activated_sandbox;
#else
  VLOG(1) << activated_sandbox;
#endif
}

bool AddResourceLimit(int resource, rlim_t limit) {
  struct rlimit old_rlimit;
  if (getrlimit(resource, &old_rlimit))
    return false;
  // Make sure we don't raise the existing limit.
  const struct rlimit new_rlimit = {
      std::min(old_rlimit.rlim_cur, limit),
      std::min(old_rlimit.rlim_max, limit)
      };
  int rc = setrlimit(resource, &new_rlimit);
  return rc == 0;
}

}  // namespace

namespace content {

LinuxSandbox::LinuxSandbox()
    : proc_fd_(-1),
      seccomp_bpf_started_(false),
      pre_initialized_(false),
      seccomp_bpf_supported_(false),
      setuid_sandbox_client_(sandbox::SetuidSandboxClient::Create()) {
  if (setuid_sandbox_client_ == NULL) {
    LOG(FATAL) << "Failed to instantiate the setuid sandbox client.";
  }
}

LinuxSandbox::~LinuxSandbox() {
}

LinuxSandbox* LinuxSandbox::GetInstance() {
  LinuxSandbox* instance = Singleton<LinuxSandbox>::get();
  CHECK(instance);
  return instance;
}

#if defined(ADDRESS_SANITIZER) && defined(OS_LINUX)
// ASan API call to notify the tool the sandbox is going to be turned on.
extern "C" void __sanitizer_sandbox_on_notify(void *reserved);
#endif

void LinuxSandbox::PreinitializeSandbox() {
  CHECK(!pre_initialized_);
  seccomp_bpf_supported_ = false;
#if defined(ADDRESS_SANITIZER) && defined(OS_LINUX)
  // ASan needs to open some resources before the sandbox is enabled.
  // This should not fork, not launch threads, not open a directory.
  __sanitizer_sandbox_on_notify(/*reserved*/NULL);
#endif

#if !defined(NDEBUG)
  // Open proc_fd_ only in Debug mode so that forgetting to close it doesn't
  // produce a sandbox escape in Release mode.
  proc_fd_ = open("/proc", O_DIRECTORY | O_RDONLY);
  CHECK(proc_fd_ >= 0);
#endif  // !defined(NDEBUG)
  // We "pre-warm" the code that detects supports for seccomp BPF.
  if (SandboxSeccompBpf::IsSeccompBpfDesired()) {
    if (!SandboxSeccompBpf::SupportsSandbox()) {
      VLOG(1) << "Lacking support for seccomp-bpf sandbox.";
    } else {
      seccomp_bpf_supported_ = true;
    }
  }
  pre_initialized_ = true;
}

bool LinuxSandbox::InitializeSandbox() {
  bool seccomp_bpf_started = false;
  LinuxSandbox* linux_sandbox = LinuxSandbox::GetInstance();
  // We need to make absolutely sure that our sandbox is "sealed" before
  // InitializeSandbox does exit.
  base::ScopedClosureRunner sandbox_sealer(
      base::Bind(&LinuxSandbox::SealSandbox, base::Unretained(linux_sandbox)));
  const std::string process_type =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);

  // No matter what, it's always an error to call InitializeSandbox() after
  // threads have been created.
  if (!linux_sandbox->IsSingleThreaded()) {
    std::string error_message = "InitializeSandbox() called with multiple "
                                "threads in process " + process_type;
    // The GPU process is allowed to call InitializeSandbox() with threads for
    // now, because it loads third party libraries.
    if (process_type != switches::kGpuProcess)
      DCHECK(false) << error_message;
    LOG(ERROR) << error_message;
    return false;
  }

  // Attempt to limit the future size of the address space of the process.
  linux_sandbox->LimitAddressSpace(process_type);

  // First, try to enable seccomp-bpf.
  seccomp_bpf_started = linux_sandbox->StartSeccompBpf(process_type);

  return seccomp_bpf_started;
}

int LinuxSandbox::GetStatus() const {
  CHECK(pre_initialized_);
  int sandbox_flags = 0;
  if (setuid_sandbox_client_->IsSandboxed()) {
    sandbox_flags |= kSandboxLinuxSUID;
    if (setuid_sandbox_client_->IsInNewPIDNamespace())
      sandbox_flags |= kSandboxLinuxPIDNS;
    if (setuid_sandbox_client_->IsInNewNETNamespace())
      sandbox_flags |= kSandboxLinuxNetNS;
  }

  if (seccomp_bpf_supported() &&
      SandboxSeccompBpf::ShouldEnableSeccompBpf(switches::kRendererProcess)) {
    // We report whether the sandbox will be activated when renderers go
    // through sandbox initialization.
    sandbox_flags |= kSandboxLinuxSeccompBpf;
  }

  return sandbox_flags;
}

// Threads are counted via /proc/self/task. This is a little hairy because of
// PID namespaces and existing sandboxes, so "self" must really be used instead
// of using the pid.
bool LinuxSandbox::IsSingleThreaded() const {
  struct stat task_stat;
  int fstat_ret;
  if (proc_fd_ >= 0) {
    // If a handle to /proc is available, use it. This allows to bypass file
    // system restrictions.
    fstat_ret = fstatat(proc_fd_, "self/task/", &task_stat, 0);
  } else {
    // Otherwise, make an attempt to access the file system directly.
    fstat_ret = fstatat(AT_FDCWD, "/proc/self/task/", &task_stat, 0);
  }
  // In Debug mode, it's mandatory to be able to count threads to catch bugs.
#if !defined(NDEBUG)
  // Using DCHECK here would be incorrect. DCHECK can be enabled in non
  // official release mode.
  CHECK_EQ(0, fstat_ret) << "Could not count threads, the sandbox was not "
                         << "pre-initialized properly.";
#endif  // !defined(NDEBUG)
  if (fstat_ret) {
    // Pretend to be monothreaded if it can't be determined (for instance the
    // setuid sandbox is already engaged but no proc_fd_ is available).
    return true;
  }

  // At least "..", "." and the current thread should be present.
  CHECK_LE(3UL, task_stat.st_nlink);
  // Counting threads via /proc/self/task could be racy. For the purpose of
  // determining if the current proces is monothreaded it works: if at any
  // time it becomes monothreaded, it'll stay so.
  return task_stat.st_nlink == 3;
}

bool LinuxSandbox::seccomp_bpf_started() const {
  return seccomp_bpf_started_;
}

sandbox::SetuidSandboxClient*
    LinuxSandbox::setuid_sandbox_client() const {
  return setuid_sandbox_client_.get();
}

// For seccomp-bpf, we use the SandboxSeccompBpf class.
bool LinuxSandbox::StartSeccompBpf(const std::string& process_type) {
  CHECK(!seccomp_bpf_started_);
  if (!pre_initialized_)
    PreinitializeSandbox();
  if (seccomp_bpf_supported())
    seccomp_bpf_started_ = SandboxSeccompBpf::StartSandbox(process_type);

  if (seccomp_bpf_started_)
    LogSandboxStarted("seccomp-bpf");

  return seccomp_bpf_started_;
}

bool LinuxSandbox::seccomp_bpf_supported() const {
  CHECK(pre_initialized_);
  return seccomp_bpf_supported_;
}

bool LinuxSandbox::LimitAddressSpace(const std::string& process_type) {
  (void) process_type;
#if !defined(ADDRESS_SANITIZER)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNoSandbox)) {
    return false;
  }

  // Limit the address space to 4GB.
  // This is in the hope of making some kernel exploits more complex and less
  // reliable. It also limits sprays a little on 64-bit.
  rlim_t address_space_limit = std::numeric_limits<uint32_t>::max();
#if defined(__LP64__)
  // On 64 bits, V8 and possibly others will reserve massive memory ranges and
  // rely on on-demand paging for allocation.  Unfortunately, even
  // MADV_DONTNEED ranges  count towards RLIMIT_AS so this is not an option.
  // See crbug.com/169327 for a discussion.
  // For now, increase limit to 16GB for renderer and worker processes to
  // accomodate.
  if (process_type == switches::kRendererProcess ||
      process_type == switches::kWorkerProcess) {
    address_space_limit = 1L << 34;
  }
#endif  // defined(__LP64__)

  // On all platforms, add a limit to the brk() heap that would prevent
  // allocations that can't be index by an int.
  const rlim_t kNewDataSegmentMaxSize = std::numeric_limits<int>::max();

  bool limited_as = AddResourceLimit(RLIMIT_AS, address_space_limit);
  bool limited_data = AddResourceLimit(RLIMIT_DATA, kNewDataSegmentMaxSize);
  return limited_as && limited_data;
#else
  return false;
#endif  // !defined(ADDRESS_SANITIZER)
}

void LinuxSandbox::SealSandbox() {
  if (proc_fd_ >= 0) {
    int ret = HANDLE_EINTR(close(proc_fd_));
    CHECK_EQ(0, ret);
    proc_fd_ = -1;
  }
}

}  // namespace content

