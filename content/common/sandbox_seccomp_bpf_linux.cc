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

#if defined(__arm__) && !defined(MAP_STACK)
#define MAP_STACK 0x20000  // Daisy build environment has old headers.
#endif

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
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
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/linux_syscalls.h"

using playground2::arch_seccomp_data;
using playground2::ErrorCode;
using playground2::Sandbox;
using sandbox::BrokerProcess;
// TODO(jln): remove once the cleanup in crbug.com/325535 is done.
using namespace sandbox;

namespace {

void StartSandboxWithPolicy(Sandbox::EvaluateSyscall syscall_policy,
                            BrokerProcess* broker_process);

inline bool RunningOnASAN() {
#if defined(ADDRESS_SANITIZER)
  return true;
#else
  return false;
#endif
}

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

// Write |error_message| to stderr. Similar to RawLog(), but a bit more careful
// about async-signal safety. |size| is the size to write and should typically
// not include a terminating \0.
void WriteToStdErr(const char* error_message, size_t size) {
  while (size > 0) {
    // TODO(jln): query the current policy to check if send() is available and
    // use it to perform a non blocking write.
    const int ret = HANDLE_EINTR(write(STDERR_FILENO, error_message, size));
    // We can't handle any type of error here.
    if (ret <= 0 || static_cast<size_t>(ret) > size) break;
    size -= ret;
    error_message += ret;
  }
}

// Print a seccomp-bpf failure to handle |sysno| to stderr in an
// async-signal safe way.
void PrintSyscallError(uint32_t sysno) {
  if (sysno >= 1024)
    sysno = 0;
  // TODO(markus): replace with async-signal safe snprintf when available.
  const size_t kNumDigits = 4;
  char sysno_base10[kNumDigits];
  uint32_t rem = sysno;
  uint32_t mod = 0;
  for (int i = kNumDigits - 1; i >= 0; i--) {
    mod = rem % 10;
    rem /= 10;
    sysno_base10[i] = '0' + mod;
  }
  static const char kSeccompErrorPrefix[] =
      __FILE__":**CRASHING**:seccomp-bpf failure in syscall ";
  static const char kSeccompErrorPostfix[] = "\n";
  WriteToStdErr(kSeccompErrorPrefix, sizeof(kSeccompErrorPrefix) - 1);
  WriteToStdErr(sysno_base10, sizeof(sysno_base10));
  WriteToStdErr(kSeccompErrorPostfix, sizeof(kSeccompErrorPostfix) - 1);
}

intptr_t CrashSIGSYS_Handler(const struct arch_seccomp_data& args, void* aux) {
  uint32_t syscall = args.nr;
  if (syscall >= 1024)
    syscall = 0;
  PrintSyscallError(syscall);

  // Encode 8-bits of the 1st two arguments too, so we can discern which socket
  // type, which fcntl, ... etc., without being likely to hit a mapped
  // address.
  // Do not encode more bits here without thinking about increasing the
  // likelihood of collision with mapped pages.
  syscall |= ((args.args[0] & 0xffUL) << 12);
  syscall |= ((args.args[1] & 0xffUL) << 20);
  // Purposefully dereference the syscall as an address so it'll show up very
  // clearly and easily in crash dumps.
  volatile char* addr = reinterpret_cast<volatile char*>(syscall);
  *addr = '\0';
  // In case we hit a mapped address, hit the null page with just the syscall,
  // for paranoia.
  syscall &= 0xfffUL;
  addr = reinterpret_cast<volatile char*>(syscall);
  *addr = '\0';
  for (;;)
    _exit(1);
}

// TODO(jln): rewrite reporting functions.
intptr_t SIGSYSCloneFailure(const struct arch_seccomp_data& args, void* aux) {
  // "flags" in the first argument in the kernel's clone().
  // Mark as volatile to be able to find the value on the stack in a minidump.
#if !defined(NDEBUG)
  RAW_LOG(ERROR, __FILE__":**CRASHING**:clone() failure\n");
#endif
  volatile uint64_t clone_flags = args.args[0];
  volatile char* addr;
  if (IsArchitectureX86_64()) {
    addr = reinterpret_cast<volatile char*>(clone_flags & 0xFFFFFF);
    *addr = '\0';
  }
  // Hit the NULL page if this fails to fault.
  addr = reinterpret_cast<volatile char*>(clone_flags & 0xFFF);
  *addr = '\0';
  for (;;)
    _exit(1);
}

// TODO(jln): rewrite reporting functions.
intptr_t SIGSYSPrctlFailure(const struct arch_seccomp_data& args,
                            void* /* aux */) {
  // Mark as volatile to be able to find the value on the stack in a minidump.
#if !defined(NDEBUG)
  RAW_LOG(ERROR, __FILE__":**CRASHING**:prctl() failure\n");
#endif
  volatile uint64_t option = args.args[0];
  volatile char* addr =
      reinterpret_cast<volatile char*>(option & 0xFFF);
  *addr = '\0';
  for (;;)
    _exit(1);
}

intptr_t SIGSYSIoctlFailure(const struct arch_seccomp_data& args,
                            void* /* aux */) {
  // Make "request" volatile so that we can see it on the stack in a minidump.
#if !defined(NDEBUG)
  RAW_LOG(ERROR, __FILE__":**CRASHING**:ioctl() failure\n");
#endif
  volatile uint64_t request = args.args[1];
  volatile char* addr = reinterpret_cast<volatile char*>(request & 0xFFFF);
  *addr = '\0';
  // Hit the NULL page if this fails.
  addr = reinterpret_cast<volatile char*>(request & 0xFFF);
  *addr = '\0';
  for (;;)
    _exit(1);
}

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

bool IsBaselinePolicyAllowed(int sysno) {
  if (IsAllowedAddressSpaceAccess(sysno) ||
      IsAllowedBasicScheduler(sysno) ||
      IsAllowedEpoll(sysno) ||
      IsAllowedFileSystemAccessViaFd(sysno) ||
      IsAllowedGeneralIo(sysno) ||
      IsAllowedGetOrModifySocket(sysno) ||
      IsAllowedGettime(sysno) ||
      IsAllowedPrctl(sysno) ||
      IsAllowedProcessStartOrDeath(sysno) ||
      IsAllowedSignalHandling(sysno) ||
      IsFutex(sysno) ||
      IsGetSimpleId(sysno) ||
      IsKernelInternalApi(sysno) ||
#if defined(__arm__)
      IsArmPrivate(sysno) ||
#endif
      IsKill(sysno) ||
      IsAllowedOperationOnFd(sysno)) {
    return true;
  } else {
    return false;
  }
}

// System calls that will trigger the crashing SIGSYS handler.
bool IsBaselinePolicyWatched(int sysno) {
  if (IsAdminOperation(sysno) ||
      IsAdvancedScheduler(sysno) ||
      IsAdvancedTimer(sysno) ||
      IsAsyncIo(sysno) ||
      IsDebug(sysno) ||
      IsEventFd(sysno) ||
      IsExtendedAttributes(sysno) ||
      IsFaNotify(sysno) ||
      IsFsControl(sysno) ||
      IsGlobalFSViewChange(sysno) ||
      IsGlobalProcessEnvironment(sysno) ||
      IsGlobalSystemStatus(sysno) ||
      IsInotify(sysno) ||
      IsKernelModule(sysno) ||
      IsKeyManagement(sysno) ||
      IsMessageQueue(sysno) ||
      IsMisc(sysno) ||
#if defined(__x86_64__)
      IsNetworkSocketInformation(sysno) ||
#endif
      IsNuma(sysno) ||
      IsProcessGroupOrSession(sysno) ||
      IsProcessPrivilegeChange(sysno) ||
#if defined(__i386__)
      IsSocketCall(sysno) ||  // We'll need to handle this properly to build
                              // a x86_32 policy.
#endif
#if defined(__arm__)
      IsArmPciConfig(sysno) ||
#endif
      IsTimer(sysno)) {
    return true;
  } else {
    return false;
  }
}

ErrorCode RestrictMmapFlags(Sandbox* sandbox) {
  // The flags you see are actually the allowed ones, and the variable is a
  // "denied" mask because of the negation operator.
  // Significantly, we don't permit MAP_HUGETLB, or the newer flags such as
  // MAP_POPULATE.
  // TODO(davidung), remove MAP_DENYWRITE with updated Tegra libraries.
  uint32_t denied_mask = ~(MAP_SHARED | MAP_PRIVATE | MAP_ANONYMOUS |
                           MAP_STACK | MAP_NORESERVE | MAP_FIXED |
                           MAP_DENYWRITE);
  return sandbox->Cond(3, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ANY_BITS,
                       denied_mask,
                       sandbox->Trap(CrashSIGSYS_Handler, NULL),
                       ErrorCode(ErrorCode::ERR_ALLOWED));
}

ErrorCode RestrictMprotectFlags(Sandbox* sandbox) {
  // The flags you see are actually the allowed ones, and the variable is a
  // "denied" mask because of the negation operator.
  // Significantly, we don't permit weird undocumented flags such as
  // PROT_GROWSDOWN.
  uint32_t denied_mask = ~(PROT_READ | PROT_WRITE | PROT_EXEC);
  return sandbox->Cond(2, ErrorCode::TP_32BIT, ErrorCode::OP_HAS_ANY_BITS,
                       denied_mask,
                       sandbox->Trap(CrashSIGSYS_Handler, NULL),
                       ErrorCode(ErrorCode::ERR_ALLOWED));
}

ErrorCode RestrictFcntlCommands(Sandbox* sandbox) {
  // We allow F_GETFL, F_SETFL, F_GETFD, F_SETFD, F_DUPFD, F_DUPFD_CLOEXEC,
  // F_SETLK, F_SETLKW and F_GETLK.
  // We also restrict the flags in F_SETFL. We don't want to permit flags with
  // a history of trouble such as O_DIRECT. The flags you see are actually the
  // allowed ones, and the variable is a "denied" mask because of the negation
  // operator.
  // Glibc overrides the kernel's O_LARGEFILE value. Account for this.
  int kOLargeFileFlag = O_LARGEFILE;
  if (IsArchitectureX86_64() || IsArchitectureI386())
    kOLargeFileFlag = 0100000;

  // TODO(jln): add TP_LONG/TP_SIZET types.
  ErrorCode::ArgType mask_long_type;
  if (sizeof(long) == 8)
    mask_long_type = ErrorCode::TP_64BIT;
  else if (sizeof(long) == 4)
    mask_long_type = ErrorCode::TP_32BIT;
  else
    NOTREACHED();

  unsigned long denied_mask = ~(O_ACCMODE | O_APPEND | O_NONBLOCK | O_SYNC |
                                kOLargeFileFlag | O_CLOEXEC | O_NOATIME);
  return sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_GETFL,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_SETFL,
                       sandbox->Cond(2, mask_long_type,
                                     ErrorCode::OP_HAS_ANY_BITS, denied_mask,
                                     sandbox->Trap(CrashSIGSYS_Handler, NULL),
                                     ErrorCode(ErrorCode::ERR_ALLOWED)),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_GETFD,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_SETFD,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_DUPFD,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_SETLK,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_SETLKW,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_GETLK,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT,
                       ErrorCode::OP_EQUAL, F_DUPFD_CLOEXEC,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Trap(CrashSIGSYS_Handler, NULL))))))))));
}

#if defined(__i386__)
ErrorCode RestrictSocketcallCommand(Sandbox* sandbox) {
  // Allow the same individual syscalls as we do on ARM or x86_64.
  // The main difference is that we're unable to restrict the first parameter
  // to socketpair(2). Whilst initially sounding bad, it's noteworthy that very
  // few protocols actually support socketpair(2). The scary call that we're
  // worried about, socket(2), remains blocked.
  return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_SOCKETPAIR, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_SEND, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_RECV, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_SENDTO, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_RECVFROM, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_SHUTDOWN, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_SENDMSG, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       SYS_RECVMSG, ErrorCode(ErrorCode::ERR_ALLOWED),
         ErrorCode(EPERM)))))))));
}
#endif

const int kFSDeniedErrno = EPERM;

ErrorCode BaselinePolicy(Sandbox* sandbox, int sysno) {
  if (IsBaselinePolicyAllowed(sysno)) {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }

#if defined(__x86_64__) || defined(__arm__)
  if (sysno == __NR_socketpair) {
    // Only allow AF_UNIX, PF_UNIX. Crash if anything else is seen.
    COMPILE_ASSERT(AF_UNIX == PF_UNIX, af_unix_pf_unix_different);
    return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, AF_UNIX,
                         ErrorCode(ErrorCode::ERR_ALLOWED),
                         sandbox->Trap(CrashSIGSYS_Handler, NULL));
  }
#endif

  if (sysno == __NR_madvise) {
    // Only allow MADV_DONTNEED (aka MADV_FREE).
    return sandbox->Cond(2, ErrorCode::TP_32BIT,
                         ErrorCode::OP_EQUAL, MADV_DONTNEED,
                         ErrorCode(ErrorCode::ERR_ALLOWED),
                         ErrorCode(EPERM));
  }

#if defined(__i386__) || defined(__x86_64__)
  if (sysno == __NR_mmap)
    return RestrictMmapFlags(sandbox);
#endif

#if defined(__i386__) || defined(__arm__)
  if (sysno == __NR_mmap2)
    return RestrictMmapFlags(sandbox);
#endif

  if (sysno == __NR_mprotect)
    return RestrictMprotectFlags(sandbox);

  if (sysno == __NR_fcntl)
    return RestrictFcntlCommands(sandbox);

#if defined(__i386__) || defined(__arm__)
  if (sysno == __NR_fcntl64)
    return RestrictFcntlCommands(sandbox);
#endif

  if (IsFileSystem(sysno) || IsCurrentDirectory(sysno)) {
    return ErrorCode(kFSDeniedErrno);
  }

  if (IsAnySystemV(sysno)) {
    return ErrorCode(EPERM);
  }

  if (IsUmask(sysno) || IsDeniedFileSystemAccessViaFd(sysno) ||
      IsDeniedGetOrModifySocket(sysno)) {
    return ErrorCode(EPERM);
  }

#if defined(__i386__)
  if (IsSocketCall(sysno))
    return RestrictSocketcallCommand(sandbox);
#endif

  if (IsBaselinePolicyWatched(sysno)) {
    // Previously unseen syscalls. TODO(jln): some of these should
    // be denied gracefully right away.
    return sandbox->Trap(CrashSIGSYS_Handler, NULL);
  }
  // In any other case crash the program with our SIGSYS handler.
  return sandbox->Trap(CrashSIGSYS_Handler, NULL);
}

// The BaselinePolicy only takes two arguments. BaselinePolicyWithAux
// allows us to conform to the BPF compiler's policy type.
ErrorCode BaselinePolicyWithAux(Sandbox* sandbox, int sysno, void* aux) {
  CHECK(!aux);
  return BaselinePolicy(sandbox, sysno);
}

// Main policy for x86_64/i386. Extended by ArmGpuProcessPolicy.
ErrorCode GpuProcessPolicy(Sandbox* sandbox, int sysno,
                           void* broker_process) {
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
      return sandbox->Trap(GpuSIGSYS_Handler, broker_process);
    default:
      if (IsEventFd(sysno))
        return ErrorCode(ErrorCode::ERR_ALLOWED);

      // Default on the baseline policy.
      return BaselinePolicy(sandbox, sysno);
  }
}

// x86_64/i386.
// A GPU broker policy is the same as a GPU policy with open and
// openat allowed.
ErrorCode GpuBrokerProcessPolicy(Sandbox* sandbox, int sysno, void* aux) {
  // "aux" would typically be NULL, when called from
  // "EnableGpuBrokerPolicyCallBack"
  switch (sysno) {
    case __NR_access:
    case __NR_open:
    case __NR_openat:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    default:
      return GpuProcessPolicy(sandbox, sysno, aux);
  }
}

// Generic ARM GPU process sandbox, inheriting from GpuProcessPolicy.
ErrorCode ArmGpuProcessPolicy(Sandbox* sandbox, int sysno,
                              void* broker_process) {
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
      if (IsAdvancedScheduler(sysno))
        return ErrorCode(ErrorCode::ERR_ALLOWED);

      // Default to the generic GPU policy.
      return GpuProcessPolicy(sandbox, sysno, broker_process);
  }
}

// Same as above but with shmat allowed, inheriting from GpuProcessPolicy.
ErrorCode ArmGpuProcessPolicyWithShmat(Sandbox* sandbox, int sysno,
                                       void* broker_process) {
#if defined(__arm__)
  if (sysno == __NR_shmat)
    return ErrorCode(ErrorCode::ERR_ALLOWED);
#endif  // defined(__arm__)

  return ArmGpuProcessPolicy(sandbox, sysno, broker_process);
}

// A GPU broker policy is the same as a GPU policy with open and
// openat allowed.
ErrorCode ArmGpuBrokerProcessPolicy(Sandbox* sandbox,
                                    int sysno, void* aux) {
  // "aux" would typically be NULL, when called from
  // "EnableGpuBrokerPolicyCallBack"
  switch (sysno) {
    case __NR_access:
    case __NR_open:
    case __NR_openat:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    default:
      return ArmGpuProcessPolicy(sandbox, sysno, aux);
  }
}

// Allow clone(2) for threads.
// Reject fork(2) attempts with EPERM.
// Crash if anything else is attempted.
// Don't restrict on ASAN.
ErrorCode RestrictCloneToThreadsAndEPERMFork(Sandbox* sandbox) {
  // Glibc's pthread.
  if (!RunningOnASAN()) {
    return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                         CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
                         CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS |
                         CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID,
                         ErrorCode(ErrorCode::ERR_ALLOWED),
           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                         CLONE_PARENT_SETTID | SIGCHLD,
                         ErrorCode(EPERM),
           // ARM
           sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                         CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | SIGCHLD,
                         ErrorCode(EPERM),
           sandbox->Trap(SIGSYSCloneFailure, NULL))));
  } else {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

ErrorCode RestrictPrctl(Sandbox* sandbox) {
  // Allow PR_SET_NAME, PR_SET_DUMPABLE, PR_GET_DUMPABLE. Will need to add
  // seccomp compositing in the future.
  // PR_SET_PTRACER is used by breakpad but not needed anymore.
  return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       PR_SET_NAME, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       PR_SET_DUMPABLE, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       PR_GET_DUMPABLE, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Trap(SIGSYSPrctlFailure, NULL))));
}

ErrorCode RestrictIoctl(Sandbox* sandbox) {
  // Allow TCGETS and FIONREAD, trap to SIGSYSIoctlFailure otherwise.
  return sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, TCGETS,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, FIONREAD,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
                       sandbox->Trap(SIGSYSIoctlFailure, NULL)));
}

ErrorCode RendererOrWorkerProcessPolicy(Sandbox* sandbox, int sysno, void*) {
  switch (sysno) {
    case __NR_clone:
      return RestrictCloneToThreadsAndEPERMFork(sandbox);
    case __NR_ioctl:
      return RestrictIoctl(sandbox);
    case __NR_prctl:
      return RestrictPrctl(sandbox);
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
        if (IsSystemVSharedMemory(sysno))
          return ErrorCode(ErrorCode::ERR_ALLOWED);
#endif
#if defined(__i386__)
        if (IsSystemVIpc(sysno))
          return ErrorCode(ErrorCode::ERR_ALLOWED);
#endif
      }

      // Default on the baseline policy.
      return BaselinePolicy(sandbox, sysno);
  }
}

ErrorCode FlashProcessPolicy(Sandbox* sandbox, int sysno, void*) {
  switch (sysno) {
    case __NR_clone:
      return RestrictCloneToThreadsAndEPERMFork(sandbox);
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
        if (IsSystemVSharedMemory(sysno))
          return ErrorCode(ErrorCode::ERR_ALLOWED);
#endif
#if defined(__i386__)
        if (IsSystemVIpc(sysno))
          return ErrorCode(ErrorCode::ERR_ALLOWED);
#endif
      }

      // Default on the baseline policy.
      return BaselinePolicy(sandbox, sysno);
  }
}

ErrorCode BlacklistDebugAndNumaPolicy(Sandbox* sandbox, int sysno, void*) {
  if (!Sandbox::IsValidSyscallNumber(sysno)) {
    // TODO(jln) we should not have to do that in a trivial policy.
    return ErrorCode(ENOSYS);
  }

  if (IsDebug(sysno) || IsNuma(sysno))
    return sandbox->Trap(CrashSIGSYS_Handler, NULL);

  return ErrorCode(ErrorCode::ERR_ALLOWED);
}

// Allow all syscalls.
// This will still deny x32 or IA32 calls in 64 bits mode or
// 64 bits system calls in compatibility mode.
ErrorCode AllowAllPolicy(Sandbox*, int sysno, void*) {
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
    CHECK_EQ(kFSDeniedErrno, errno);

    // We should never allow the creation of netlink sockets.
    syscall_ret = socket(AF_NETLINK, SOCK_DGRAM, 0);
    CHECK_EQ(-1, syscall_ret);
    CHECK_EQ(EPERM, errno);
#endif  // !defined(NDEBUG)
  }
}

bool EnableGpuBrokerPolicyCallback() {
  StartSandboxWithPolicy(GpuBrokerProcessPolicy, NULL);
  return true;
}

bool EnableArmGpuBrokerPolicyCallback() {
  StartSandboxWithPolicy(ArmGpuBrokerProcessPolicy, NULL);
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
void InitGpuBrokerProcess(Sandbox::EvaluateSyscall gpu_policy,
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

  if (gpu_policy == ArmGpuProcessPolicy ||
      gpu_policy == ArmGpuProcessPolicyWithShmat) {
    // We shouldn't be using this policy on non-ARM architectures.
    CHECK(IsArchitectureArm());
    AddArmGpuWhitelist(&read_whitelist, &write_whitelist);
    sandbox_callback = EnableArmGpuBrokerPolicyCallback;
  } else if (gpu_policy == GpuProcessPolicy) {
    sandbox_callback = EnableGpuBrokerPolicyCallback;
  } else {
    // We shouldn't be initializing a GPU broker process without a GPU process
    // policy.
    NOTREACHED();
  }

  *broker_process = new BrokerProcess(kFSDeniedErrno,
                                      read_whitelist, write_whitelist);
  // Initialize the broker process and give it a sandbox callback.
  CHECK((*broker_process)->Init(sandbox_callback));
}

// Warms up/preloads resources needed by the policies.
// Eventually start a broker process and return it in broker_process.
void WarmupPolicy(Sandbox::EvaluateSyscall policy,
                  BrokerProcess** broker_process) {
  if (policy == GpuProcessPolicy) {
    // Create a new broker process.
    InitGpuBrokerProcess(policy, broker_process);

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
  } else if (policy == ArmGpuProcessPolicy ||
             policy == ArmGpuProcessPolicyWithShmat) {
    // Create a new broker process.
    InitGpuBrokerProcess(policy, broker_process);

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

Sandbox::EvaluateSyscall GetProcessSyscallPolicy(
    const CommandLine& command_line,
    const std::string& process_type) {
  if (process_type == switches::kGpuProcess) {
    // On Chrome OS ARM, we need a specific GPU process policy.
    if (IsChromeOS() && IsArchitectureArm()) {
      if (command_line.HasSwitch(switches::kGpuSandboxAllowSysVShm))
        return ArmGpuProcessPolicyWithShmat;
      else
        return ArmGpuProcessPolicy;
    }
    else
      return GpuProcessPolicy;
  }

  if (process_type == switches::kPpapiPluginProcess) {
    // TODO(jln): figure out what to do with non-Flash PPAPI
    // out-of-process plug-ins.
    return FlashProcessPolicy;
  }

  if (process_type == switches::kRendererProcess ||
      process_type == switches::kWorkerProcess) {
    return RendererOrWorkerProcessPolicy;
  }

  if (process_type == switches::kUtilityProcess) {
    // TODO(jorgelo): review sandbox initialization in utility_main.cc if we
    // change this policy.
    return BlacklistDebugAndNumaPolicy;
  }

  NOTREACHED();
  // This will be our default if we need one.
  return AllowAllPolicy;
}

// broker_process can be NULL if there is no need for one.
void StartSandboxWithPolicy(Sandbox::EvaluateSyscall syscall_policy,
                            BrokerProcess* broker_process) {
  // Starting the sandbox is a one-way operation. The kernel doesn't allow
  // us to unload a sandbox policy after it has been started. Nonetheless,
  // in order to make the use of the "Sandbox" object easier, we allow for
  // the object to be destroyed after the sandbox has been started. Note that
  // doing so does not stop the sandbox.
  Sandbox sandbox;
  sandbox.SetSandboxPolicyDeprecated(syscall_policy, broker_process);
  sandbox.StartSandbox();
}

// Initialize the seccomp-bpf sandbox.
bool StartBpfSandbox(const CommandLine& command_line,
                     const std::string& process_type) {
  Sandbox::EvaluateSyscall syscall_policy =
      GetProcessSyscallPolicy(command_line, process_type);

  BrokerProcess* broker_process = NULL;
  // Warm up resources needed by the policy we're about to enable and
  // eventually start a broker process.
  WarmupPolicy(syscall_policy, &broker_process);

  StartSandboxWithPolicy(syscall_policy, broker_process);

  RunSandboxSanityChecks(process_type);

  return true;
}

}  // namespace

#endif  // SECCOMP_BPF_SANDBOX

namespace content {

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
    playground2::BpfSandboxPolicy policy) {
#if defined(SECCOMP_BPF_SANDBOX)
  if (IsSeccompBpfDesired() && SupportsSandbox()) {
    CHECK(policy);
    StartSandboxWithPolicy(policy, NULL);
    return true;
  }
#endif  // defined(SECCOMP_BPF_SANDBOX)
  return false;
}

#if defined(SECCOMP_BPF_SANDBOX)
playground2::BpfSandboxPolicyCallback SandboxSeccompBpf::GetBaselinePolicy() {
  return base::Bind(&BaselinePolicyWithAux);
}
#endif  // defined(SECCOMP_BPF_SANDBOX)

}  // namespace content
