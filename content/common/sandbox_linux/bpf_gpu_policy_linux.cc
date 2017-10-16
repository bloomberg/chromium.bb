// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/bpf_gpu_policy_linux.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/common/sandbox_init_gpu_linux.h"
#include "content/common/sandbox_linux/sandbox_bpf_base_policy_linux.h"
#include "content/common/sandbox_linux/sandbox_seccomp_bpf_linux.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/linux/syscall_broker/broker_process.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

using sandbox::arch_seccomp_data;
using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::bpf_dsl::Trap;
using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::BrokerProcess;
using sandbox::SyscallSets;

namespace content {
namespace {

intptr_t GpuSIGSYS_Handler(const struct arch_seccomp_data& args,
                           void* aux_broker_process) {
  RAW_CHECK(aux_broker_process);
  BrokerProcess* broker_process =
      static_cast<BrokerProcess*>(aux_broker_process);
  switch (args.nr) {
#if !defined(__aarch64__)
    case __NR_access:
      return broker_process->Access(reinterpret_cast<const char*>(args.args[0]),
                                    static_cast<int>(args.args[1]));
    case __NR_open:
#if defined(MEMORY_SANITIZER)
      // http://crbug.com/372840
      __msan_unpoison_string(reinterpret_cast<const char*>(args.args[0]));
#endif
      return broker_process->Open(reinterpret_cast<const char*>(args.args[0]),
                                  static_cast<int>(args.args[1]));
#endif  // !defined(__aarch64__)
    case __NR_faccessat:
      if (static_cast<int>(args.args[0]) == AT_FDCWD) {
        return broker_process->Access(
            reinterpret_cast<const char*>(args.args[1]),
            static_cast<int>(args.args[2]));
      } else {
        return -EPERM;
      }
    case __NR_openat:
      // Allow using openat() as open().
      if (static_cast<int>(args.args[0]) == AT_FDCWD) {
        return broker_process->Open(reinterpret_cast<const char*>(args.args[1]),
                                    static_cast<int>(args.args[2]));
      } else {
        return -EPERM;
      }
    default:
      RAW_CHECK(false);
      return -ENOSYS;
  }
}

}  // namespace

GpuProcessPolicy::GpuProcessPolicy() : broker_process_(NULL) {}

GpuProcessPolicy::~GpuProcessPolicy() {}

// Main policy for x86_64/i386. Extended by CrosArmGpuProcessPolicy.
ResultExpr GpuProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
#if !defined(OS_CHROMEOS)
    case __NR_ftruncate:
#endif
    case __NR_ioctl:
      return Allow();
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__)
    // The Nvidia driver uses flags not in the baseline policy
    // (MAP_LOCKED | MAP_EXECUTABLE | MAP_32BIT)
    case __NR_mmap:
#endif
    // We also hit this on the linux_chromeos bot but don't yet know what
    // weird flags were involved.
    case __NR_mprotect:
    // TODO(jln): restrict prctl.
    case __NR_prctl:
    case __NR_sysinfo:
      return Allow();
#if !defined(__aarch64__)
    case __NR_access:
    case __NR_open:
#endif  // !defined(__aarch64__)
    case __NR_faccessat:
    case __NR_openat:
      DCHECK(broker_process_);
      return Trap(GpuSIGSYS_Handler, broker_process_);
    case __NR_sched_getaffinity:
    case __NR_sched_setaffinity:
      return sandbox::RestrictSchedTarget(GetPolicyPid(), sysno);
    default:
      if (SyscallSets::IsEventFd(sysno))
        return Allow();

      // Default on the baseline policy.
      return SandboxBPFBasePolicy::EvaluateSyscall(sysno);
  }
}

bool GpuProcessPolicy::PreSandboxHook() {
  return GpuPreSandboxHook(this);
}

GpuBrokerProcessPolicy::GpuBrokerProcessPolicy() {}

GpuBrokerProcessPolicy::~GpuBrokerProcessPolicy() {}

// x86_64/i386 or desktop ARM.
// A GPU broker policy is the same as a GPU policy with access, open,
// openat and in the non-Chrome OS case unlink allowed.
ResultExpr GpuBrokerProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
#if !defined(__aarch64__)
    case __NR_access:
    case __NR_open:
#endif  // !defined(__aarch64__)
    case __NR_faccessat:
    case __NR_openat:
#if !defined(OS_CHROMEOS) && !defined(__aarch64__)
    // The broker process needs to able to unlink the temporary
    // files that it may create. This is used by DRI3.
    case __NR_unlink:
#endif
      return Allow();
    default:
      return GpuProcessPolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace content
