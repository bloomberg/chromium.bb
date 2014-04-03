// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A mini-zygote specifically for Native Client.

#include "components/nacl/loader/nacl_helper_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/global_descriptors.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/process/kill.h"
#include "base/rand_util.h"
#include "components/nacl/loader/nacl_listener.h"
#include "components/nacl/loader/nacl_sandbox_linux.h"
#include "content/public/common/zygote_fork_delegate_linux.h"
#include "crypto/nss_util.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_switches.h"
#include "sandbox/linux/services/libc_urandom_override.h"

namespace {

struct NaClLoaderSystemInfo {
  size_t prereserved_sandbox_size;
  long number_of_cores;
};

// The child must mimic the behavior of zygote_main_linux.cc on the child
// side of the fork. See zygote_main_linux.cc:HandleForkRequest from
//   if (!child) {
void BecomeNaClLoader(const std::vector<int>& child_fds,
                      const NaClLoaderSystemInfo& system_info,
                      bool uses_nonsfi_mode) {
  VLOG(1) << "NaCl loader: setting up IPC descriptor";
  // don't need zygote FD any more
  if (IGNORE_EINTR(close(kNaClZygoteDescriptor)) != 0)
    LOG(ERROR) << "close(kNaClZygoteDescriptor) failed.";
  bool sandbox_initialized = InitializeBPFSandbox();
  if (!sandbox_initialized) {
    LOG(ERROR) << "Could not initialize NaCl's second "
      << "layer sandbox (seccomp-bpf).";
  }
  base::GlobalDescriptors::GetInstance()->Set(
      kPrimaryIPCChannel,
      child_fds[content::ZygoteForkDelegate::kBrowserFDIndex]);

  base::MessageLoopForIO main_message_loop;
  NaClListener listener;
  listener.set_uses_nonsfi_mode(uses_nonsfi_mode);
  listener.set_prereserved_sandbox_size(system_info.prereserved_sandbox_size);
  listener.set_number_of_cores(system_info.number_of_cores);
  listener.Listen();
  _exit(0);
}

// Start the NaCl loader in a child created by the NaCl loader Zygote.
void ChildNaClLoaderInit(const std::vector<int>& child_fds,
                         const NaClLoaderSystemInfo& system_info,
                         bool uses_nonsfi_mode) {
  const int parent_fd = child_fds[content::ZygoteForkDelegate::kParentFDIndex];
  const int dummy_fd = child_fds[content::ZygoteForkDelegate::kDummyFDIndex];
  bool validack = false;
  const size_t kMaxReadSize = 1024;
  char buffer[kMaxReadSize];
  // Wait until the parent process has discovered our PID.  We
  // should not fork any child processes (which the seccomp
  // sandbox does) until then, because that can interfere with the
  // parent's discovery of our PID.
  const int nread = HANDLE_EINTR(read(parent_fd, buffer, kMaxReadSize));
  const std::string switch_prefix = std::string("--") +
      switches::kProcessChannelID + std::string("=");
  const size_t len = switch_prefix.length();

  if (nread < 0) {
    perror("read");
    LOG(ERROR) << "read returned " << nread;
  } else if (nread > static_cast<int>(len)) {
    if (switch_prefix.compare(0, len, buffer, 0, len) == 0) {
      VLOG(1) << "NaCl loader is synchronised with Chrome zygote";
      CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kProcessChannelID,
          std::string(&buffer[len], nread - len));
      validack = true;
    }
  }
  if (IGNORE_EINTR(close(dummy_fd)) != 0)
    LOG(ERROR) << "close(dummy_fd) failed";
  if (IGNORE_EINTR(close(parent_fd)) != 0)
    LOG(ERROR) << "close(parent_fd) failed";
  if (validack) {
    BecomeNaClLoader(child_fds, system_info, uses_nonsfi_mode);
  } else {
    LOG(ERROR) << "Failed to synch with zygote";
  }
  _exit(1);
}

// Handle a fork request from the Zygote.
// Some of this code was lifted from
// content/browser/zygote_main_linux.cc:ForkWithRealPid()
bool HandleForkRequest(const std::vector<int>& child_fds,
                       const NaClLoaderSystemInfo& system_info,
                       PickleIterator* input_iter,
                       Pickle* output_pickle) {
  bool uses_nonsfi_mode;
  if (!input_iter->ReadBool(&uses_nonsfi_mode)) {
    LOG(ERROR) << "Could not read uses_nonsfi_mode status";
    return false;
  }

  if (content::ZygoteForkDelegate::kNumPassedFDs != child_fds.size()) {
    LOG(ERROR) << "nacl_helper: unexpected number of fds, got "
        << child_fds.size();
    return false;
  }

  VLOG(1) << "nacl_helper: forking";
  pid_t child_pid = fork();
  if (child_pid < 0) {
    PLOG(ERROR) << "*** fork() failed.";
  }

  if (child_pid == 0) {
    ChildNaClLoaderInit(child_fds, system_info, uses_nonsfi_mode);
    NOTREACHED();
  }

  // I am the parent.
  // First, close the dummy_fd so the sandbox won't find me when
  // looking for the child's pid in /proc. Also close other fds.
  for (size_t i = 0; i < child_fds.size(); i++) {
    if (IGNORE_EINTR(close(child_fds[i])) != 0)
      LOG(ERROR) << "close(child_fds[i]) failed";
  }
  VLOG(1) << "nacl_helper: child_pid is " << child_pid;

  // Now send child_pid (eventually -1 if fork failed) to the Chrome Zygote.
  output_pickle->WriteInt(child_pid);
  return true;
}

bool HandleGetTerminationStatusRequest(PickleIterator* input_iter,
                                       Pickle* output_pickle) {
  pid_t child_to_wait;
  if (!input_iter->ReadInt(&child_to_wait)) {
    LOG(ERROR) << "Could not read pid to wait for";
    return false;
  }

  bool known_dead;
  if (!input_iter->ReadBool(&known_dead)) {
    LOG(ERROR) << "Could not read known_dead status";
    return false;
  }
  // TODO(jln): With NaCl, known_dead seems to never be set to true (unless
  // called from the Zygote's kZygoteCommandReap command). This means that we
  // will sometimes detect the process as still running when it's not. Fix
  // this!

  int exit_code;
  base::TerminationStatus status;
  if (known_dead)
    status = base::GetKnownDeadTerminationStatus(child_to_wait, &exit_code);
  else
    status = base::GetTerminationStatus(child_to_wait, &exit_code);
  output_pickle->WriteInt(static_cast<int>(status));
  output_pickle->WriteInt(exit_code);
  return true;
}

// This is a poor man's check on whether we are sandboxed.
bool IsSandboxed() {
  int proc_fd = open("/proc/self/exe", O_RDONLY);
  if (proc_fd >= 0) {
    close(proc_fd);
    return false;
  }
  return true;
}

// Honor a command |command_type|. Eventual command parameters are
// available in |input_iter| and eventual file descriptors attached to
// the command are in |attached_fds|.
// Reply to the command on |reply_fds|.
bool HonorRequestAndReply(int reply_fd,
                          int command_type,
                          const std::vector<int>& attached_fds,
                          const NaClLoaderSystemInfo& system_info,
                          PickleIterator* input_iter) {
  Pickle write_pickle;
  bool have_to_reply = false;
  // Commands must write anything to send back to |write_pickle|.
  switch (command_type) {
    case nacl::kNaClForkRequest:
      have_to_reply = HandleForkRequest(attached_fds, system_info,
                                        input_iter, &write_pickle);
      break;
    case nacl::kNaClGetTerminationStatusRequest:
      have_to_reply =
          HandleGetTerminationStatusRequest(input_iter, &write_pickle);
      break;
    default:
      LOG(ERROR) << "Unsupported command from Zygote";
      return false;
  }
  if (!have_to_reply)
    return false;
  const std::vector<int> empty;  // We never send file descriptors back.
  if (!UnixDomainSocket::SendMsg(reply_fd, write_pickle.data(),
                                 write_pickle.size(), empty)) {
    LOG(ERROR) << "*** send() to zygote failed";
    return false;
  }
  return true;
}

// Read a request from the Zygote from |zygote_ipc_fd| and handle it.
// Die on EOF from |zygote_ipc_fd|.
bool HandleZygoteRequest(int zygote_ipc_fd,
                         const NaClLoaderSystemInfo& system_info) {
  std::vector<int> fds;
  char buf[kNaClMaxIPCMessageLength];
  const ssize_t msglen = UnixDomainSocket::RecvMsg(zygote_ipc_fd,
      &buf, sizeof(buf), &fds);
  // If the Zygote has started handling requests, we should be sandboxed via
  // the setuid sandbox.
  if (!IsSandboxed()) {
    LOG(ERROR) << "NaCl helper process running without a sandbox!\n"
      << "Most likely you need to configure your SUID sandbox "
      << "correctly";
  }
  if (msglen == 0 || (msglen == -1 && errno == ECONNRESET)) {
    // EOF from the browser. Goodbye!
    _exit(0);
  }
  if (msglen < 0) {
    PLOG(ERROR) << "nacl_helper: receive from zygote failed";
    return false;
  }

  Pickle read_pickle(buf, msglen);
  PickleIterator read_iter(read_pickle);
  int command_type;
  if (!read_iter.ReadInt(&command_type)) {
    LOG(ERROR) << "Unable to read command from Zygote";
    return false;
  }
  return HonorRequestAndReply(zygote_ipc_fd, command_type, fds, system_info,
                              &read_iter);
}

static const char kNaClHelperReservedAtZero[] = "reserved_at_zero";
static const char kNaClHelperRDebug[] = "r_debug";

// Since we were started by nacl_helper_bootstrap rather than in the
// usual way, the debugger cannot figure out where our executable
// or the dynamic linker or the shared libraries are in memory,
// so it won't find any symbols.  But we can fake it out to find us.
//
// The zygote passes --r_debug=0xXXXXXXXXXXXXXXXX.
// nacl_helper_bootstrap replaces the Xs with the address of its _r_debug
// structure.  The debugger will look for that symbol by name to
// discover the addresses of key dynamic linker data structures.
// Since all it knows about is the original main executable, which
// is the bootstrap program, it finds the symbol defined there.  The
// dynamic linker's structure is somewhere else, but it is filled in
// after initialization.  The parts that really matter to the
// debugger never change.  So we just copy the contents of the
// dynamic linker's structure into the address provided by the option.
// Hereafter, if someone attaches a debugger (or examines a core dump),
// the debugger will find all the symbols in the normal way.
static void CheckRDebug(char* argv0) {
  std::string r_debug_switch_value =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kNaClHelperRDebug);
  if (!r_debug_switch_value.empty()) {
    char* endp;
    uintptr_t r_debug_addr = strtoul(r_debug_switch_value.c_str(), &endp, 0);
    if (r_debug_addr != 0 && *endp == '\0') {
      r_debug* bootstrap_r_debug = reinterpret_cast<r_debug*>(r_debug_addr);
      *bootstrap_r_debug = _r_debug;

      // Since the main executable (the bootstrap program) does not
      // have a dynamic section, the debugger will not skip the
      // first element of the link_map list as it usually would for
      // an executable or PIE that was loaded normally.  But the
      // dynamic linker has set l_name for the PIE to "" as is
      // normal for the main executable.  So the debugger doesn't
      // know which file it is.  Fill in the actual file name, which
      // came in as our argv[0].
      link_map* l = _r_debug.r_map;
      if (l->l_name[0] == '\0')
        l->l_name = argv0;
    }
  }
}

// The zygote passes --reserved_at_zero=0xXXXXXXXXXXXXXXXX.
// nacl_helper_bootstrap replaces the Xs with the amount of prereserved
// sandbox memory.
//
// CheckReservedAtZero parses the value of the argument reserved_at_zero
// and returns the amount of prereserved sandbox memory.
static size_t CheckReservedAtZero() {
  size_t prereserved_sandbox_size = 0;
  std::string reserved_at_zero_switch_value =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kNaClHelperReservedAtZero);
  if (!reserved_at_zero_switch_value.empty()) {
    char* endp;
    prereserved_sandbox_size =
        strtoul(reserved_at_zero_switch_value.c_str(), &endp, 0);
    if (*endp != '\0')
      LOG(ERROR) << "Could not parse reserved_at_zero argument value of "
                 << reserved_at_zero_switch_value;
  }
  return prereserved_sandbox_size;
}

}  // namespace

#if defined(ADDRESS_SANITIZER)
// Do not install the SIGSEGV handler in ASan. This should make the NaCl
// platform qualification test pass.
static const char kAsanDefaultOptionsNaCl[] = "handle_segv=0";

// Override the default ASan options for the NaCl helper.
// __asan_default_options should not be instrumented, because it is called
// before ASan is initialized.
extern "C"
__attribute__((no_sanitize_address))
// The function isn't referenced from the executable itself. Make sure it isn't
// stripped by the linker.
__attribute__((used))
__attribute__((visibility("default")))
const char* __asan_default_options() {
  return kAsanDefaultOptionsNaCl;
}
#endif

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;
  base::RandUint64();  // acquire /dev/urandom fd before sandbox is raised
  // Allows NSS to fopen() /dev/urandom.
  sandbox::InitLibcUrandomOverrides();
#if defined(USE_NSS)
  // Configure NSS for use inside the NaCl process.
  // The fork check has not caused problems for NaCl, but this appears to be
  // best practice (see other places LoadNSSLibraries is called.)
  crypto::DisableNSSForkCheck();
  // Without this line on Linux, HMAC::Init will instantiate a singleton that
  // in turn attempts to open a file.  Disabling this behavior avoids a ~70 ms
  // stall the first time HMAC is used.
  crypto::ForceNSSNoDBInit();
  // Load shared libraries before sandbox is raised.
  // NSS is needed to perform hashing for validation caching.
  crypto::LoadNSSLibraries();
#endif
  const NaClLoaderSystemInfo system_info = {
    CheckReservedAtZero(),
    sysconf(_SC_NPROCESSORS_ONLN)
  };

  CheckRDebug(argv[0]);

  // Check that IsSandboxed() works. We should not be sandboxed at this point.
  CHECK(!IsSandboxed()) << "Unexpectedly sandboxed!";

  const std::vector<int> empty;
  // Send the zygote a message to let it know we are ready to help
  if (!UnixDomainSocket::SendMsg(kNaClZygoteDescriptor,
                                 kNaClHelperStartupAck,
                                 sizeof(kNaClHelperStartupAck), empty)) {
    LOG(ERROR) << "*** send() to zygote failed";
  }

  // Now handle requests from the Zygote.
  while (true) {
    bool request_handled = HandleZygoteRequest(kNaClZygoteDescriptor,
                                               system_info);
    // Do not turn this into a CHECK() without thinking about robustness
    // against malicious IPC requests.
    DCHECK(request_handled);
  }
  NOTREACHED();
}
