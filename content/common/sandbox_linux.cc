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
#include "content/common/seccomp_sandbox.h"
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

// Implement the command line enabling logic for seccomp-legacy.
bool IsSeccompLegacyDesired() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNoSandbox)) {
    return false;
  }
#if defined(SECCOMP_SANDBOX)
#if defined(NDEBUG)
  // Off by default. Allow turning on with a switch.
  return command_line->HasSwitch(switches::kEnableSeccompSandbox);
#else
  // On by default. Allow turning off with a switch.
  return !command_line->HasSwitch(switches::kDisableSeccompSandbox);
#endif  // NDEBUG
#endif  // SECCOMP_SANDBOX
  return false;
}

// Our "policy" on whether or not to enable seccomp-legacy. Only renderers are
// supported.
bool ShouldEnableSeccompLegacy(const std::string& process_type) {
  if (IsSeccompLegacyDesired() &&
      process_type == switches::kRendererProcess) {
    return true;
  } else {
    return false;
  }
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
      seccomp_legacy_supported_(false),
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
  seccomp_legacy_supported_ = false;
  seccomp_bpf_supported_ = false;
#if defined(ADDRESS_SANITIZER) && defined(OS_LINUX)
  // ASan needs to open some resources before the sandbox is enabled.
  // This should not fork, not launch threads, not open a directory.
  __sanitizer_sandbox_on_notify(/*reserved*/NULL);
#endif
#if defined(SECCOMP_SANDBOX)
  if (IsSeccompLegacyDesired()) {
    proc_fd_ = open("/proc", O_DIRECTORY | O_RDONLY);
    if (proc_fd_ < 0) {
      LOG(ERROR) << "Cannot access \"/proc\". Disabling seccomp-legacy "
                    "sandboxing.";
      // Now is a good time to figure out if we can support seccomp sandboxing
      // at all. We will call SupportsSeccompSandbox again later, when actually
      // enabling it, but we allow the implementation to cache some information.
      // This is the only place where we will log full lack of seccomp-legacy
      // support.
    } else if (!SupportsSeccompSandbox(proc_fd_)) {
      VLOG(1) << "Lacking support for seccomp-legacy sandbox.";
      CHECK_EQ(HANDLE_EINTR(close(proc_fd_)), 0);
      proc_fd_ = -1;
    } else {
      seccomp_legacy_supported_ = true;
    }
  }
#endif  // SECCOMP_SANDBOX
  // Similarly, we "pre-warm" the code that detects supports for seccomp BPF.
  // TODO(jln): Use proc_fd_ here too once we're comfortable it does not create
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

// Once we finally know our process type, we can cleanup proc_fd_
// or pass it to seccomp-legacy.
void LinuxSandbox::PreinitializeSandboxFinish(
    const std::string& process_type) {
  CHECK(pre_initialized_);
  if (proc_fd_ >= 0) {
    if (ShouldEnableSeccompLegacy(process_type)) {
#if defined(SECCOMP_SANDBOX)
      SeccompSandboxSetProcFd(proc_fd_);
#endif
    } else {
      DCHECK_GE(proc_fd_, 0);
      CHECK_EQ(HANDLE_EINTR(close(proc_fd_)), 0);
    }
    proc_fd_ = -1;
  }
}

void LinuxSandbox::PreinitializeSandbox(const std::string& process_type) {
  PreinitializeSandboxBegin();
  PreinitializeSandboxFinish(process_type);
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

  // We only try to enable seccomp-legacy when seccomp-bpf is not supported
  // or not enabled.
  if (!(sandbox_flags & kSandboxLinuxSeccompBpf) &&
      seccomp_legacy_supported() &&
      ShouldEnableSeccompLegacy(switches::kRendererProcess)) {
    // Same here, what we report is what we will do for the renderer.
    sandbox_flags |= kSandboxLinuxSeccompLegacy;
  }
  return sandbox_flags;
}

bool LinuxSandbox::IsSingleThreaded() const {
  // TODO(jln): re-implement this properly and use our proc_fd_ if available.
  // Possibly racy, but it's ok because this is more of a debug check to catch
  // new threaded situations arising during development.
  int num_threads = file_util::CountFilesCreatedAfter(
      FilePath("/proc/self/task"),
      base::Time::UnixEpoch());

  // We pass the test if we don't know ( == 0), because the setuid sandbox
  // will prevent /proc access in some contexts.
  return num_threads == 1 || num_threads == 0;
}

bool LinuxSandbox::seccomp_bpf_started() const {
  return seccomp_bpf_started_;
}

sandbox::SetuidSandboxClient*
    LinuxSandbox::setuid_sandbox_client() const {
  return setuid_sandbox_client_.get();
}

// For seccomp-legacy, we implement the policy inline, here.
bool LinuxSandbox::StartSeccompLegacy(const std::string& process_type) {
  if (!pre_initialized_)
    PreinitializeSandbox(process_type);
  if (seccomp_legacy_supported() && ShouldEnableSeccompLegacy(process_type)) {
    // SupportsSeccompSandbox() returns a cached result, as we already
    // called it earlier in the PreinitializeSandbox(). Thus, it is OK for us
    // to not pass in a file descriptor for "/proc".
#if defined(SECCOMP_SANDBOX)
    if (SupportsSeccompSandbox(-1)) {
      StartSeccompSandbox();
      LogSandboxStarted("seccomp-legacy");
      return true;
    }
#endif
  }
  return false;
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

bool LinuxSandbox::seccomp_legacy_supported() const {
  CHECK(pre_initialized_);
  return seccomp_legacy_supported_;
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
#if defined(__LP64__)
  // On 64 bits, limit the address space to 16GB. This is in the hope of making
  // some kernel exploits more complex and less reliable.  This limit has to be
  // very high because V8 and possibly others will reserve memory ranges and
  // rely on on-demand paging for allocation.  Unfortunately, even
  // MADV_DONTNEED ranges  count towards RLIMIT_AS so this is not an option.
  // See crbug.com/169327 for a discussion.
  const rlim_t kNewAddressSpaceMaxSize = 1L << 34;
#else
  // On 32 bits, enforce the 4GB limit. On a 64 bits kernel, this could
  // prevent far calling to 64 bits and abuse the memory allocator to exploit
  // a kernel vulnerability.
  const rlim_t kNewAddressSpaceMaxSize = std::numeric_limits<uint32_t>::max();
#endif  // defined(__LP64__)
  // On all platforms, add a limit to the brk() heap that would prevent
  // allocations that can't be index by an int.
  const rlim_t kNewDataSegmentMaxSize = std::numeric_limits<int>::max();

  bool limited_as = AddResourceLimit(RLIMIT_AS, kNewAddressSpaceMaxSize);
  bool limited_data = AddResourceLimit(RLIMIT_DATA, kNewDataSegmentMaxSize);
  return limited_as && limited_data;
#else
  return false;
#endif  // !defined(ADDRESS_SANITIZER)
}

}  // namespace content

