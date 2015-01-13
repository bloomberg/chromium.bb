// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <setjmp.h>
#include <sys/resource.h>
#include <sys/syscall.h>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/third_party/valgrind/valgrind.h"
#include "build/build_config.h"

namespace base {

namespace {

const int kForegroundPriority = 0;

#if defined(OS_CHROMEOS)
// We are more aggressive in our lowering of background process priority
// for chromeos as we have much more control over other processes running
// on the machine.
//
// TODO(davemoore) Refactor this by adding support for higher levels to set
// the foregrounding / backgrounding process so we don't have to keep
// chrome / chromeos specific logic here.
const int kBackgroundPriority = 19;
const char kControlPath[] = "/sys/fs/cgroup/cpu%s/cgroup.procs";
const char kForeground[] = "/chrome_renderers/foreground";
const char kBackground[] = "/chrome_renderers/background";
const char kProcPath[] = "/proc/%d/cgroup";

struct CGroups {
  // Check for cgroups files. ChromeOS supports these by default. It creates
  // a cgroup mount in /sys/fs/cgroup and then configures two cpu task groups,
  // one contains at most a single foreground renderer and the other contains
  // all background renderers. This allows us to limit the impact of background
  // renderers on foreground ones to a greater level than simple renicing.
  bool enabled;
  base::FilePath foreground_file;
  base::FilePath background_file;

  CGroups() {
    foreground_file =
        base::FilePath(base::StringPrintf(kControlPath, kForeground));
    background_file =
        base::FilePath(base::StringPrintf(kControlPath, kBackground));
    base::FileSystemType foreground_type;
    base::FileSystemType background_type;
    enabled =
        base::GetFileSystemType(foreground_file, &foreground_type) &&
        base::GetFileSystemType(background_file, &background_type) &&
        foreground_type == FILE_SYSTEM_CGROUP &&
        background_type == FILE_SYSTEM_CGROUP;
  }
};

base::LazyInstance<CGroups> cgroups = LAZY_INSTANCE_INITIALIZER;
#else
const int kBackgroundPriority = 5;
#endif

struct CheckForNicePermission {
  CheckForNicePermission() : can_reraise_priority(false) {
    // We won't be able to raise the priority if we don't have the right rlimit.
    // The limit may be adjusted in /etc/security/limits.conf for PAM systems.
    struct rlimit rlim;
    if ((getrlimit(RLIMIT_NICE, &rlim) == 0) &&
        (20 - kForegroundPriority) <= static_cast<int>(rlim.rlim_cur)) {
        can_reraise_priority = true;
    }
  };

  bool can_reraise_priority;
};

bool IsRunningOnValgrind() {
  return RUNNING_ON_VALGRIND;
}

// This function runs on the stack specified on the clone call. It uses longjmp
// to switch back to the original stack so the child can return from sys_clone.
int CloneHelper(void* arg) {
  jmp_buf* env_ptr = reinterpret_cast<jmp_buf*>(arg);
  longjmp(*env_ptr, 1);

  // Should not be reached.
  RAW_CHECK(false);
  return 1;
}

// This function is noinline to ensure that stack_buf is below the stack pointer
// that is saved when setjmp is called below. This is needed because when
// compiled with FORTIFY_SOURCE, glibc's longjmp checks that the stack is moved
// upwards. See crbug.com/442912 for more details.
#if defined(ADDRESS_SANITIZER)
// Disable AddressSanitizer instrumentation for this function to make sure
// |stack_buf| is allocated on thread stack instead of ASan's fake stack.
// Under ASan longjmp() will attempt to clean up the area between the old and
// new stack pointers and print a warning that may confuse the user.
__attribute__((no_sanitize_address))
#endif
NOINLINE pid_t CloneAndLongjmpInChild(unsigned long flags,
                                      pid_t* ptid,
                                      pid_t* ctid,
                                      jmp_buf* env) {
  // We use the libc clone wrapper instead of making the syscall
  // directly because making the syscall may fail to update the libc's
  // internal pid cache. The libc interface unfortunately requires
  // specifying a new stack, so we use setjmp/longjmp to emulate
  // fork-like behavior.
  char stack_buf[PTHREAD_STACK_MIN];
#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM_FAMILY) || \
    defined(ARCH_CPU_MIPS64_FAMILY) || defined(ARCH_CPU_MIPS_FAMILY)
  // The stack grows downward.
  void* stack = stack_buf + sizeof(stack_buf);
#else
#error "Unsupported architecture"
#endif
  return clone(&CloneHelper, stack, flags, env, ptid, nullptr, ctid);
}

}  // namespace

// static
bool Process::CanBackgroundProcesses() {
#if defined(OS_CHROMEOS)
  if (cgroups.Get().enabled)
    return true;
#endif

  static LazyInstance<CheckForNicePermission> check_for_nice_permission =
      LAZY_INSTANCE_INITIALIZER;
  return check_for_nice_permission.Get().can_reraise_priority;
}

bool Process::IsProcessBackgrounded() const {
  DCHECK(IsValid());

#if defined(OS_CHROMEOS)
  if (cgroups.Get().enabled) {
    std::string proc;
    if (base::ReadFileToString(
            base::FilePath(StringPrintf(kProcPath, process_)),
            &proc)) {
      std::vector<std::string> proc_parts;
      base::SplitString(proc, ':', &proc_parts);
      DCHECK(proc_parts.size() == 3);
      bool ret = proc_parts[2] == std::string(kBackground);
      return ret;
    } else {
      return false;
    }
  }
#endif
  return GetPriority() == kBackgroundPriority;
}

bool Process::SetProcessBackgrounded(bool background) {
  DCHECK(IsValid());

#if defined(OS_CHROMEOS)
  if (cgroups.Get().enabled) {
    std::string pid = StringPrintf("%d", process_);
    const base::FilePath file =
        background ?
            cgroups.Get().background_file : cgroups.Get().foreground_file;
    return base::WriteFile(file, pid.c_str(), pid.size()) > 0;
  }
#endif // OS_CHROMEOS

  if (!CanBackgroundProcesses())
    return false;

  int priority = background ? kBackgroundPriority : kForegroundPriority;
  int result = setpriority(PRIO_PROCESS, process_, priority);
  DPCHECK(result == 0);
  return result == 0;
}

pid_t ForkWithFlags(unsigned long flags, pid_t* ptid, pid_t* ctid) {
  const bool clone_tls_used = flags & CLONE_SETTLS;
  const bool invalid_ctid =
      (flags & (CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID)) && !ctid;
  const bool invalid_ptid = (flags & CLONE_PARENT_SETTID) && !ptid;

  // We do not support CLONE_VM.
  const bool clone_vm_used = flags & CLONE_VM;

  if (clone_tls_used || invalid_ctid || invalid_ptid || clone_vm_used) {
    RAW_LOG(FATAL, "Invalid usage of ForkWithFlags");
  }

  // Valgrind's clone implementation does not support specifiying a child_stack
  // without CLONE_VM, so we cannot use libc's clone wrapper when running under
  // Valgrind. As a result, the libc pid cache may be incorrect under Valgrind.
  // See crbug.com/442817 for more details.
  if (IsRunningOnValgrind()) {
    // See kernel/fork.c in Linux. There is different ordering of sys_clone
    // parameters depending on CONFIG_CLONE_BACKWARDS* configuration options.
#if defined(ARCH_CPU_X86_64)
    return syscall(__NR_clone, flags, nullptr, ptid, ctid, nullptr);
#elif defined(ARCH_CPU_X86) || defined(ARCH_CPU_ARM_FAMILY) || \
    defined(ARCH_CPU_MIPS_FAMILY) || defined(ARCH_CPU_MIPS64_FAMILY)
    // CONFIG_CLONE_BACKWARDS defined.
    return syscall(__NR_clone, flags, nullptr, ptid, nullptr, ctid);
#else
#error "Unsupported architecture"
#endif
  }

  jmp_buf env;
  if (setjmp(env) == 0) {
    return CloneAndLongjmpInChild(flags, ptid, ctid, &env);
  }

  return 0;
}

}  // namespace base
