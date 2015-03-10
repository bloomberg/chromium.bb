// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/sandbox_linux/nacl_sandbox_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <limits>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/nacl/loader/nonsfi/nonsfi_sandbox.h"
#include "components/nacl/loader/sandbox_linux/nacl_bpf_sandbox_linux.h"
#include "content/public/common/content_switches.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/namespace_sandbox.h"
#include "sandbox/linux/services/proc_util.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"

#if !defined(OS_NACL_NONSFI)
#include "sandbox/linux/services/resource_limits.h"
#endif

namespace nacl {

namespace {

// This is a poor man's check on whether we are sandboxed.
bool IsSandboxed() {
  int proc_fd = open("/proc/self/exe", O_RDONLY);
  if (proc_fd >= 0) {
    PCHECK(0 == IGNORE_EINTR(close(proc_fd)));
    return false;
  }
  return true;
}

bool MaybeSetProcessNonDumpable() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kAllowSandboxDebugging)) {
    return true;
  }

  if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) != 0) {
    PLOG(ERROR) << "Failed to set non-dumpable flag";
    return false;
  }

  return prctl(PR_GET_DUMPABLE) == 0;
}

#if !defined(OS_NACL_NONSFI)
// Currently Layer-two sandbox is not yet supported on nacl_helper_nonsfi.
// This function is used only in InitializeLayerTwoSandbox().
// TODO(hidehiko): Enable the sandbox.
void RestrictAddressSpaceUsage() {
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER)
  // Sanitizers need to reserve huge chunks of the address space.
  return;
#endif

  // Add a limit to the brk() heap that would prevent allocations that can't be
  // indexed by an int. This helps working around typical security bugs.
  // This could almost certainly be set to zero. GLibc's allocator and others
  // would fall-back to mmap if brk() fails.
  const rlim_t kNewDataSegmentMaxSize = std::numeric_limits<int>::max();
  CHECK(sandbox::ResourceLimits::Lower(RLIMIT_DATA, kNewDataSegmentMaxSize));

#if defined(ARCH_CPU_64_BITS)
  // NaCl's x86-64 sandbox allocated 88GB address of space during startup:
  // - The main sandbox is 4GB
  // - There are two guard regions of 40GB each.
  // - 4GB are allocated extra to have a 4GB-aligned address.
  // See https://crbug.com/455839
  //
  // Set the limit to 128 GB and have some margin.
  const rlim_t kNewAddressSpaceLimit = 1UL << 37;
#else
  // Some architectures such as X86 allow 32 bits processes to switch to 64
  // bits when running under 64 bits kernels. Set a limit in case this happens.
  const rlim_t kNewAddressSpaceLimit = std::numeric_limits<uint32_t>::max();
#endif
  CHECK(sandbox::ResourceLimits::Lower(RLIMIT_AS, kNewAddressSpaceLimit));
}
#endif  // !OS_NACL_NONSFI

}  // namespace

NaClSandbox::NaClSandbox()
    : layer_one_enabled_(false),
      layer_one_sealed_(false),
      layer_two_enabled_(false),
      layer_two_is_nonsfi_(false),
      proc_fd_(-1),
      setuid_sandbox_client_(sandbox::SetuidSandboxClient::Create()) {
  proc_fd_.reset(
      HANDLE_EINTR(open("/proc", O_DIRECTORY | O_RDONLY | O_CLOEXEC)));
  PCHECK(proc_fd_.is_valid());
}

NaClSandbox::~NaClSandbox() {
}

bool NaClSandbox::IsSingleThreaded() {
  CHECK(proc_fd_.is_valid());
  return sandbox::ThreadHelpers::IsSingleThreaded(proc_fd_.get());
}

bool NaClSandbox::HasOpenDirectory() {
  CHECK(proc_fd_.is_valid());
  return sandbox::ProcUtil::HasOpenDirectory(proc_fd_.get());
}

void NaClSandbox::InitializeLayerOneSandbox() {
  // Check that IsSandboxed() works. We should not be sandboxed at this point.
  CHECK(!IsSandboxed()) << "Unexpectedly sandboxed!";

  if (setuid_sandbox_client_->IsSuidSandboxChild()) {
    setuid_sandbox_client_->CloseDummyFile();

    // Make sure that no directory file descriptor is open, as it would bypass
    // the setuid sandbox model.
    CHECK(!HasOpenDirectory());

    // Get sandboxed.
    CHECK(setuid_sandbox_client_->ChrootMe());
    CHECK(MaybeSetProcessNonDumpable());
    CHECK(IsSandboxed());
    layer_one_enabled_ = true;
  }
  // Currently namespace sandbox is not yet supported on nacl_helper_nonsfi.
  // TODO(hidehiko): Enable the sandbox.
#if !defined(OS_NACL_NONSFI)
  else if (sandbox::NamespaceSandbox::InNewUserNamespace()) {
    CHECK(sandbox::Credentials::MoveToNewUserNS());
    // This relies on SealLayerOneSandbox() to be called later since this
    // class is keeping a file descriptor to /proc/.
    CHECK(sandbox::Credentials::DropFileSystemAccess(proc_fd_.get()));
    CHECK(sandbox::Credentials::DropAllCapabilities(proc_fd_.get()));
    CHECK(IsSandboxed());
    layer_one_enabled_ = true;
  }
#endif  // !OS_NACL_NONSFI
}

#if !defined(OS_NACL_NONSFI)
// Currently Layer-two sandbox is not yet supported on nacl_helper_nonsfi.
// TODO(hidehiko): Enable the sandbox.
// Note that CheckForExpectedNumberOfOpenFds() is just referred from
// InitializeLayerTwoSandbox(). Enable them together.
void NaClSandbox::CheckForExpectedNumberOfOpenFds() {
  // We expect to have the following FDs open:
  //  1-3) stdin, stdout, stderr.
  //  4) The /dev/urandom FD used by base::GetUrandomFD().
  //  5) A dummy pipe FD used to overwrite kSandboxIPCChannel.
  //  6) The socket for the Chrome IPC channel that's connected to the
  //     browser process, kPrimaryIPCChannel.
  // We also have an fd for /proc (proc_fd_), but CountOpenFds excludes this.
  //
  // This sanity check ensures that dynamically loaded libraries don't
  // leave any FDs open before we enable the sandbox.
  int expected_num_fds = 6;
  if (setuid_sandbox_client_->IsSuidSandboxChild()) {
    // When using the setuid sandbox, there is one additional socket used for
    // ChrootMe(). After ChrootMe(), it is no longer connected to anything.
    ++expected_num_fds;
  }

  CHECK_EQ(expected_num_fds, sandbox::ProcUtil::CountOpenFds(proc_fd_.get()));
}

void NaClSandbox::InitializeLayerTwoSandbox(bool uses_nonsfi_mode) {
  // seccomp-bpf only applies to the current thread, so it's critical to only
  // have a single thread running here.
  DCHECK(!layer_one_sealed_);
  CHECK(IsSingleThreaded());
  CheckForExpectedNumberOfOpenFds();

  RestrictAddressSpaceUsage();

  // Pass proc_fd_ ownership to the BPF sandbox, which guarantees it will
  // be closed. There is no point in keeping it around since the BPF policy
  // will prevent its usage.
  if (uses_nonsfi_mode) {
    layer_two_enabled_ = nacl::nonsfi::InitializeBPFSandbox(proc_fd_.Pass());
    layer_two_is_nonsfi_ = true;
  } else {
    layer_two_enabled_ = nacl::InitializeBPFSandbox(proc_fd_.Pass());
  }
}
#endif  // OS_NACL_NONSFI

void NaClSandbox::SealLayerOneSandbox() {
  if (proc_fd_.is_valid() && !layer_two_enabled_) {
    // If nothing prevents us, check that there is no superfluous directory
    // open.
    CHECK(!HasOpenDirectory());
  }
  proc_fd_.reset();
  layer_one_sealed_ = true;
}

void NaClSandbox::CheckSandboxingStateWithPolicy() {
  static const char kItIsDangerousMsg[] = " this is dangerous.";
  static const char kItIsNotAllowedMsg[] =
      " this is not allowed in this configuration.";

  const bool no_sandbox_for_nonsfi_ok =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNaClDangerousNoSandboxNonSfi);
  const bool can_be_no_sandbox =
      !layer_two_is_nonsfi_ || no_sandbox_for_nonsfi_ok;

  if (!layer_one_enabled_ || !layer_one_sealed_) {
    static const char kNoSuidMsg[] =
        "The SUID sandbox is not engaged for NaCl:";
    if (can_be_no_sandbox)
      LOG(ERROR) << kNoSuidMsg << kItIsDangerousMsg;
    else
      LOG(FATAL) << kNoSuidMsg << kItIsNotAllowedMsg;
  }

#if !defined(OS_NACL_NONSFI)
  // Currently Layer-two sandbox is not yet supported on nacl_helper_nonsfi.
  // TODO(hidehiko): Enable the sandbox.
  if (!layer_two_enabled_) {
    static const char kNoBpfMsg[] =
        "The seccomp-bpf sandbox is not engaged for NaCl:";
    if (can_be_no_sandbox)
      LOG(ERROR) << kNoBpfMsg << kItIsDangerousMsg;
    else
      LOG(FATAL) << kNoBpfMsg << kItIsNotAllowedMsg;
  }
#endif
}

}  // namespace nacl
