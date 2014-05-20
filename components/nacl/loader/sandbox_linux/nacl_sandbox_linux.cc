// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/sandbox_linux/nacl_sandbox_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/nacl/loader/nonsfi/nonsfi_sandbox.h"
#include "components/nacl/loader/sandbox_linux/nacl_bpf_sandbox_linux.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"

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
  base::ScopedFD proc_self_task(HANDLE_EINTR(openat(
      proc_fd_.get(), "self/task/", O_RDONLY | O_DIRECTORY | O_CLOEXEC)));
  PCHECK(proc_self_task.is_valid());
  return sandbox::ThreadHelpers::IsSingleThreaded(proc_self_task.get());
}

bool NaClSandbox::HasOpenDirectory() {
  CHECK(proc_fd_.is_valid());
  sandbox::Credentials credentials;
  return credentials.HasOpenDirectory(proc_fd_.get());
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
    CHECK(IsSandboxed());
    layer_one_enabled_ = true;
  }
}

void NaClSandbox::CheckForExpectedNumberOfOpenFds() {
  if (setuid_sandbox_client_->IsSuidSandboxChild()) {
    // We expect to have the following FDs open:
    //  1-3) stdin, stdout, stderr.
    //  4) The /dev/urandom FD used by base::GetUrandomFD().
    //  5) A dummy pipe FD used to overwrite kSandboxIPCChannel.
    //  6) The socket created by the SUID sandbox helper, used by ChrootMe().
    //     After ChrootMe(), this is no longer connected to anything.
    //     (Only present when running under the SUID sandbox.)
    //  7) The socket for the Chrome IPC channel that's connected to the
    //     browser process, kPrimaryIPCChannel.
    //
    // This sanity check ensures that dynamically loaded libraries don't
    // leave any FDs open before we enable the sandbox.
    sandbox::Credentials credentials;
    CHECK_EQ(7, credentials.CountOpenFds(proc_fd_.get()));
  }
}

void NaClSandbox::InitializeLayerTwoSandbox(bool uses_nonsfi_mode) {
  // seccomp-bpf only applies to the current thread, so it's critical to only
  // have a single thread running here.
  DCHECK(!layer_one_sealed_);
  CHECK(IsSingleThreaded());
  CheckForExpectedNumberOfOpenFds();

  if (uses_nonsfi_mode) {
    layer_two_enabled_ = nacl::nonsfi::InitializeBPFSandbox();
    layer_two_is_nonsfi_ = true;
  } else {
    layer_two_enabled_ = nacl::InitializeBPFSandbox();
  }
}

void NaClSandbox::SealLayerOneSandbox() {
  if (!layer_two_enabled_) {
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
      CommandLine::ForCurrentProcess()->HasSwitch(
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

  if (!layer_two_enabled_) {
    static const char kNoBpfMsg[] =
        "The seccomp-bpf sandbox is not engaged for NaCl:";
    if (can_be_no_sandbox)
      LOG(ERROR) << kNoBpfMsg << kItIsDangerousMsg;
    else
      LOG(FATAL) << kNoBpfMsg << kItIsNotAllowedMsg;
  }
}

}  // namespace nacl
