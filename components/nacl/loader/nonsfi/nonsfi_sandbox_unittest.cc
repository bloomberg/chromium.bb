// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Sanitizers internally use some syscalls which non-SFI NaCl disallows.
#if !defined(ADDRESS_SANITIZER) && !defined(THREAD_SANITIZER) && \
    !defined(MEMORY_SANITIZER) && !defined(LEAK_SANITIZER)

#include "components/nacl/loader/nonsfi/nonsfi_sandbox.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/services/linux_syscalls.h"
#include "third_party/lss/linux_syscall_support.h"  // for MAKE_PROCESS_CPUCLOCK

namespace {

void DoPipe(base::ScopedFD* fds) {
  int tmp_fds[2];
  BPF_ASSERT_EQ(0, pipe(tmp_fds));
  fds[0].reset(tmp_fds[0]);
  fds[1].reset(tmp_fds[1]);
}

void DoSocketpair(base::ScopedFD* fds) {
  int tmp_fds[2];
  BPF_ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, tmp_fds));
  fds[0].reset(tmp_fds[0]);
  fds[1].reset(tmp_fds[1]);
}

TEST(NaClNonSfiSandboxTest, BPFIsSupported) {
  bool seccomp_bpf_supported = (
      sandbox::SandboxBPF::SupportsSeccompSandbox(-1) ==
      sandbox::SandboxBPF::STATUS_AVAILABLE);
  if (!seccomp_bpf_supported) {
    LOG(ERROR) << "Seccomp BPF is not supported, these tests "
               << "will pass without running";
  }
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 invalid_sysno,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  syscall(999);
}

const int kExpectedValue = 123;

void* SetValueInThread(void* test_val_ptr) {
  *reinterpret_cast<int*>(test_val_ptr) = kExpectedValue;
  return NULL;
}

// To make this test pass, we need to allow sched_getaffinity and
// mmap. We just disable this test not to complicate the sandbox.
BPF_TEST_C(NaClNonSfiSandboxTest,
           clone_by_pthread_create,
           nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  // clone call for thread creation is allowed.
  pthread_t th;
  int test_val = 42;
  BPF_ASSERT_EQ(0, pthread_create(&th, NULL, &SetValueInThread, &test_val));
  BPF_ASSERT_EQ(0, pthread_join(th, NULL));
  BPF_ASSERT_EQ(kExpectedValue, test_val);
}

int DoFork() {
  // Call clone() to do a fork().
  const int pid = syscall(__NR_clone, SIGCHLD, NULL);
  if (pid == 0)
    _exit(0);
  return pid;
}

// The sanity check for DoFork without the sandbox.
TEST(NaClNonSfiSandboxTest, DoFork) {
  const int pid = DoFork();
  ASSERT_LT(0, pid);
  int status;
  ASSERT_EQ(pid, HANDLE_EINTR(waitpid(pid, &status, 0)));
  ASSERT_TRUE(WIFEXITED(status));
  ASSERT_EQ(0, WEXITSTATUS(status));
}

// Then, try this in the sandbox.
BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 clone_for_fork,
                 DEATH_MESSAGE(sandbox::GetCloneErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  DoFork();
}

BPF_TEST_C(NaClNonSfiSandboxTest,
           prctl_SET_NAME,
           nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_prctl, PR_SET_NAME, "foo"));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 prctl_SET_DUMPABLE,
                 DEATH_MESSAGE(sandbox::GetPrctlErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  syscall(__NR_prctl, PR_SET_DUMPABLE, 1UL);
}

BPF_TEST_C(NaClNonSfiSandboxTest,
           socketcall_allowed,
           nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  base::ScopedFD fds[2];
  struct msghdr msg = {};
  struct iovec iov;
  std::string payload("foo");
  iov.iov_base = &payload[0];
  iov.iov_len = payload.size();
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  DoSocketpair(fds);
  BPF_ASSERT_EQ(static_cast<int>(payload.size()),
                HANDLE_EINTR(sendmsg(fds[1].get(), &msg, 0)));
  BPF_ASSERT_EQ(static_cast<int>(payload.size()),
                HANDLE_EINTR(recvmsg(fds[0].get(), &msg, 0)));
  BPF_ASSERT_EQ(0, shutdown(fds[0].get(), SHUT_RDWR));
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 accept,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  accept(0, NULL, NULL);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 bind,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  bind(0, NULL, 0);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 connect,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  connect(0, NULL, 0);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 getpeername,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  getpeername(0, NULL, NULL);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 getsockname,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  struct sockaddr addr;
  socklen_t addrlen = 0;
  getsockname(0, &addr, &addrlen);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 getsockopt,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  getsockopt(0, 0, 0, NULL, NULL);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 listen,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  listen(0, 0);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 recv,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  recv(0, NULL, 0, 0);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 recvfrom,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  recvfrom(0, NULL, 0, 0, NULL, NULL);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 send,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  send(0, NULL, 0, 0);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 sendto,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  sendto(0, NULL, 0, 0, NULL, 0);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 setsockopt,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  setsockopt(0, 0, 0, NULL, 0);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 socket,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  socket(0, 0, 0);
}

#if defined(__x86_64__) || defined(__arm__)
BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 socketpair,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  int fds[2];
  socketpair(AF_INET, SOCK_STREAM, 0, fds);
}
#endif

BPF_TEST_C(NaClNonSfiSandboxTest,
           fcntl_SETFD_allowed,
           nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  base::ScopedFD fds[2];
  DoSocketpair(fds);
  BPF_ASSERT_EQ(0, fcntl(fds[0].get(), F_SETFD, FD_CLOEXEC));
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 fcntl_SETFD,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  base::ScopedFD fds[2];
  DoSocketpair(fds);
  fcntl(fds[0].get(), F_SETFD, 99);
}

BPF_TEST_C(NaClNonSfiSandboxTest,
           fcntl_GETFL_SETFL_allowed,
           nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  base::ScopedFD fds[2];
  DoPipe(fds);
  const int fd = fds[0].get();
  BPF_ASSERT_EQ(0, fcntl(fd, F_GETFL));
  BPF_ASSERT_EQ(0, fcntl(fd, F_SETFL, O_RDWR | O_NONBLOCK));
  BPF_ASSERT_EQ(O_NONBLOCK, fcntl(fd, F_GETFL));
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 fcntl_GETFL_SETFL,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  base::ScopedFD fds[2];
  DoSocketpair(fds);
  fcntl(fds[0].get(), F_SETFL, O_APPEND);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 fcntl_DUPFD,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  fcntl(0, F_DUPFD);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 fcntl_DUPFD_CLOEXEC,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  fcntl(0, F_DUPFD_CLOEXEC);
}

void* DoAllowedAnonymousMmap() {
  return mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
              MAP_ANONYMOUS | MAP_SHARED, -1, 0);
}

BPF_TEST_C(NaClNonSfiSandboxTest,
           mmap_allowed,
           nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  void* ptr = DoAllowedAnonymousMmap();
  BPF_ASSERT_NE(MAP_FAILED, ptr);
  BPF_ASSERT_EQ(0, munmap(ptr, getpagesize()));
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 mmap_unallowed_flag,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
       MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 mmap_unallowed_prot,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  mmap(NULL, getpagesize(), PROT_READ | PROT_GROWSDOWN,
       MAP_ANONYMOUS, -1, 0);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 mmap_exec,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  mmap(NULL, getpagesize(), PROT_EXEC, MAP_ANONYMOUS, -1, 0);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 mmap_read_exec,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  mmap(NULL, getpagesize(), PROT_READ | PROT_EXEC, MAP_ANONYMOUS, -1, 0);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 mmap_write_exec,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  mmap(NULL, getpagesize(), PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS, -1, 0);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 mmap_read_write_exec,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC,
       MAP_ANONYMOUS, -1, 0);
}

BPF_TEST_C(NaClNonSfiSandboxTest,
           mprotect_allowed,
           nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  void* ptr = DoAllowedAnonymousMmap();
  BPF_ASSERT_NE(MAP_FAILED, ptr);
  BPF_ASSERT_EQ(0, mprotect(ptr, getpagesize(), PROT_READ));
  BPF_ASSERT_EQ(0, munmap(ptr, getpagesize()));
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 mprotect_unallowed_prot,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  // We have tested DoAllowedAnonymousMmap is allowed in
  // mmap_allowed, so we can make sure the following mprotect call
  // kills the process.
  void* ptr = DoAllowedAnonymousMmap();
  BPF_ASSERT_NE(MAP_FAILED, ptr);
  mprotect(ptr, getpagesize(), PROT_READ | PROT_GROWSDOWN);
}

BPF_TEST_C(NaClNonSfiSandboxTest,
           brk,
           nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  char* next_brk = static_cast<char*>(sbrk(0)) + getpagesize();
  // The kernel interface must return zero for brk.
  BPF_ASSERT_EQ(0, syscall(__NR_brk, next_brk));
  // The libc wrapper translates it to ENOMEM.
  errno = 0;
  BPF_ASSERT_EQ(-1, brk(next_brk));
  BPF_ASSERT_EQ(ENOMEM, errno);
}

void CheckClock(clockid_t clockid) {
  struct timespec ts;
  ts.tv_sec = ts.tv_nsec = -1;
  BPF_ASSERT_EQ(0, clock_gettime(clockid, &ts));
  BPF_ASSERT_LE(0, ts.tv_sec);
  BPF_ASSERT_LE(0, ts.tv_nsec);
}

BPF_TEST_C(NaClNonSfiSandboxTest,
           clock_gettime_allowed,
           nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  CheckClock(CLOCK_MONOTONIC);
  CheckClock(CLOCK_PROCESS_CPUTIME_ID);
  CheckClock(CLOCK_REALTIME);
  CheckClock(CLOCK_THREAD_CPUTIME_ID);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 clock_gettime_crash_monotonic_raw,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
}

#if defined(OS_CHROMEOS)

// A custom BPF tester delegate to run IsRunningOnChromeOS() before
// the sandbox is enabled because we cannot run it with non-SFI BPF
// sandbox enabled.
class ClockSystemTesterDelegate : public sandbox::BPFTesterDelegate {
 public:
  ClockSystemTesterDelegate()
      : is_running_on_chromeos_(base::SysInfo::IsRunningOnChromeOS()) {}
  virtual ~ClockSystemTesterDelegate() {}

  virtual scoped_ptr<sandbox::SandboxBPFPolicy> GetSandboxBPFPolicy() OVERRIDE {
    return scoped_ptr<sandbox::SandboxBPFPolicy>(
        new nacl::nonsfi::NaClNonSfiBPFSandboxPolicy());
  }
  virtual void RunTestFunction() OVERRIDE {
    if (is_running_on_chromeos_) {
      CheckClock(base::TimeTicks::kClockSystemTrace);
    } else {
      struct timespec ts;
      // kClockSystemTrace is 11, which is CLOCK_THREAD_CPUTIME_ID of
      // the init process (pid=1). If kernel supports this feature,
      // this may succeed even if this is not running on Chrome OS. We
      // just check this clock_gettime call does not crash.
      clock_gettime(base::TimeTicks::kClockSystemTrace, &ts);
    }
  }

 private:
  const bool is_running_on_chromeos_;
  DISALLOW_COPY_AND_ASSIGN(ClockSystemTesterDelegate);
};

BPF_TEST_D(BPFTest, BPFTestWithDelegateClass, ClockSystemTesterDelegate);

#else

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 clock_gettime_crash_system_trace,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  struct timespec ts;
  clock_gettime(base::TimeTicks::kClockSystemTrace, &ts);
}

#endif  // defined(OS_CHROMEOS)

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 clock_gettime_crash_cpu_clock,
                 DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  // We can't use clock_getcpuclockid() because it's not implemented in newlib,
  // and it might not work inside the sandbox anyway.
  const pid_t kInitPID = 1;
  const clockid_t kInitCPUClockID =
      MAKE_PROCESS_CPUCLOCK(kInitPID, CPUCLOCK_SCHED);

  struct timespec ts;
  clock_gettime(kInitCPUClockID, &ts);
}

BPF_DEATH_TEST_C(NaClNonSfiSandboxTest,
                 invalid_syscall_crash,
                 DEATH_SEGV_MESSAGE(sandbox::GetErrorMessageContentForTests()),
                 nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {
  sandbox::Syscall::InvalidCall();
}

// The following test cases check if syscalls return EPERM regardless
// of arguments.
#define RESTRICT_SYSCALL_EPERM_TEST(name)                      \
  BPF_TEST_C(NaClNonSfiSandboxTest,                            \
             name##_EPERM,                                     \
             nacl::nonsfi::NaClNonSfiBPFSandboxPolicy) {       \
    errno = 0;                                                 \
    BPF_ASSERT_EQ(-1, syscall(__NR_##name, 0, 0, 0, 0, 0, 0)); \
    BPF_ASSERT_EQ(EPERM, errno);                               \
  }

RESTRICT_SYSCALL_EPERM_TEST(epoll_create);
#if defined(__i386__) || defined(__arm__)
RESTRICT_SYSCALL_EPERM_TEST(getegid32);
RESTRICT_SYSCALL_EPERM_TEST(geteuid32);
RESTRICT_SYSCALL_EPERM_TEST(getgid32);
RESTRICT_SYSCALL_EPERM_TEST(getuid32);
#endif
RESTRICT_SYSCALL_EPERM_TEST(getegid);
RESTRICT_SYSCALL_EPERM_TEST(geteuid);
RESTRICT_SYSCALL_EPERM_TEST(getgid);
RESTRICT_SYSCALL_EPERM_TEST(getuid);
RESTRICT_SYSCALL_EPERM_TEST(madvise);
RESTRICT_SYSCALL_EPERM_TEST(open);
RESTRICT_SYSCALL_EPERM_TEST(ptrace);
RESTRICT_SYSCALL_EPERM_TEST(set_robust_list);
#if defined(__i386__) || defined(__x86_64__)
RESTRICT_SYSCALL_EPERM_TEST(time);
#endif

}  // namespace

#endif  // !ADDRESS_SANITIZER && !THREAD_SANITIZER &&
        // !MEMORY_SANITIZER && !LEAK_SANITIZER
