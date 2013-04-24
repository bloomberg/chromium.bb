// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <limits>

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

void LinuxSandbox::PreinitializeSandboxBegin() {
  CHECK(!pre_initialized_);
  seccomp_bpf_supported_ = false;
#if defined(ADDRESS_SANITIZER) && defined(OS_LINUX)
  // ASan needs to open some resources before the sandbox is enabled.
  // This should not fork, not launch threads, not open a directory.
  __sanitizer_sandbox_on_notify(/*reserved*/NULL);
#endif
  // We "pre-warm" the code that detects supports for seccomp BPF.
  // TODO(jln): Use proc_fd_ here once we're comfortable it does not create
  // an additional security risk.
  if (SandboxSeccompBpf::IsSeccompBpfDesired()) {
    if (!SandboxSeccompBpf::SupportsSandbox()) {
      VLOG(1) << "Lacking support for seccomp-bpf sandbox.";
    } else {
      seccomp_bpf_supported_ = true;
    }
  }
  pre_initialized_ = true;
}

// Once we finally know our process type, we can cleanup proc_fd_.
void LinuxSandbox::PreinitializeSandboxFinish(
    const std::string& process_type) {
  CHECK(pre_initialized_);
  // TODO(jln): move this to InitializeSandbox.
  if (proc_fd_ >= 0) {
    CHECK_EQ(HANDLE_EINTR(close(proc_fd_)), 0);
    proc_fd_ = -1;
  }
}

void LinuxSandbox::PreinitializeSandbox(const std::string& process_type) {
  PreinitializeSandboxBegin();
  PreinitializeSandboxFinish(process_type);
}

bool LinuxSandbox::InitializeSandbox() {
  bool seccomp_bpf_started = false;
  LinuxSandbox* linux_sandbox = LinuxSandbox::GetInstance();
  const std::string process_type =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);

  // No matter what, it's always an error to call InitializeSandbox() after
  // threads have been created.
  if (!linux_sandbox->IsSingleThreaded()) {
    std::string error_message = "InitializeSandbox() called with multiple "
                                "threads in process " + process_type;
    // TODO(jln): change this into a CHECK() once we are more comfortable it
    // does not trigger.
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

bool LinuxSandbox::IsSingleThreaded() const {
  // TODO(jln): re-implement this properly and use our proc_fd_ if available.
  // Possibly racy, but it's ok because this is more of a debug check to catch
  // new threaded situations arising during development.
  file_util::FileEnumerator en(base::FilePath("/proc/self/task"), false,
      file_util::FileEnumerator::FILES |
      file_util::FileEnumerator::DIRECTORIES);
  bool found_file = false;
  while (!en.Next().empty()) {
    if (found_file)
      return false;  // Found a second match.
    found_file = true;
  }

  // We pass the test if we found 0 files becase the setuid sandbox will
  // prevent /proc access in some contexts.
  return true;
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
    PreinitializeSandbox(process_type);
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

}  // namespace content

