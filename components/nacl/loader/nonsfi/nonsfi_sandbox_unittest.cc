// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include <unistd.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"

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
  bool seccomp_bpf_supported = false;
  // Seccomp-BPF tests die under TSAN v2. See http://crbug.com/356588
#if !defined(THREAD_SANITIZER)
  seccomp_bpf_supported = (
      sandbox::SandboxBPF::SupportsSeccompSandbox(-1) ==
      sandbox::SandboxBPF::STATUS_AVAILABLE);
#endif

  if (!seccomp_bpf_supported) {
    LOG(ERROR) << "Seccomp BPF is not supported, these tests "
               << "will pass without running";
  }
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, invalid_sysno,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  syscall(999);
}

const int kExpectedValue = 123;

void* SetValueInThread(void* test_val_ptr) {
  *reinterpret_cast<int*>(test_val_ptr) = kExpectedValue;
  return NULL;
}

// To make this test pass, we need to allow sched_getaffinity and
// mmap. We just disable this test not to complicate the sandbox.
BPF_TEST(NaClNonSfiSandboxTest, DISABLE_ON_ASAN(clone_by_pthread_create),
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
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
BPF_DEATH_TEST(NaClNonSfiSandboxTest, clone_for_fork,
               DEATH_MESSAGE(sandbox::GetCloneErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  DoFork();
}

BPF_TEST(NaClNonSfiSandboxTest, prctl_SET_NAME,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_prctl, PR_SET_NAME, "foo"));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, prctl_SET_DUMPABLE,
               DEATH_MESSAGE(sandbox::GetPrctlErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  syscall(__NR_prctl, PR_SET_DUMPABLE, 1UL);
}

BPF_TEST(NaClNonSfiSandboxTest, socketcall_allowed,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
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

BPF_DEATH_TEST(NaClNonSfiSandboxTest, accept,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  accept(0, NULL, NULL);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, bind,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  bind(0, NULL, 0);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, connect,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  connect(0, NULL, 0);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, getpeername,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  getpeername(0, NULL, NULL);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, getsockname,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  struct sockaddr addr;
  socklen_t addrlen = 0;
  getsockname(0, &addr, &addrlen);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, getsockopt,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  getsockopt(0, 0, 0, NULL, NULL);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, listen,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  listen(0, 0);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, recv,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  recv(0, NULL, 0, 0);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, recvfrom,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  recvfrom(0, NULL, 0, 0, NULL, NULL);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, send,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  send(0, NULL, 0, 0);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, sendto,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  sendto(0, NULL, 0, 0, NULL, 0);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, setsockopt,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  setsockopt(0, 0, 0, NULL, 0);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, socket,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  socket(0, 0, 0);
}

#if defined(__x86_64__) || defined(__arm__)
BPF_DEATH_TEST(NaClNonSfiSandboxTest, socketpair,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  int fds[2];
  socketpair(AF_INET, SOCK_STREAM, 0, fds);
}
#endif

BPF_TEST(NaClNonSfiSandboxTest, fcntl_SETFD_allowed,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  base::ScopedFD fds[2];
  DoSocketpair(fds);
  BPF_ASSERT_EQ(0, fcntl(fds[0].get(), F_SETFD, FD_CLOEXEC));
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, fcntl_SETFD,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  base::ScopedFD fds[2];
  DoSocketpair(fds);
  fcntl(fds[0].get(), F_SETFD, 99);
}

BPF_TEST(NaClNonSfiSandboxTest, fcntl_GETFL_SETFL_allowed,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  base::ScopedFD fds[2];
  DoPipe(fds);
  const int fd = fds[0].get();
  BPF_ASSERT_EQ(0, fcntl(fd, F_GETFL));
  BPF_ASSERT_EQ(0, fcntl(fd, F_SETFL, O_RDWR | O_NONBLOCK));
  BPF_ASSERT_EQ(O_NONBLOCK, fcntl(fd, F_GETFL));
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, fcntl_GETFL_SETFL,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  base::ScopedFD fds[2];
  DoSocketpair(fds);
  fcntl(fds[0].get(), F_SETFL, O_APPEND);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, fcntl_DUPFD,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  fcntl(0, F_DUPFD);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, fcntl_DUPFD_CLOEXEC,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  fcntl(0, F_DUPFD_CLOEXEC);
}

void* DoAllowedAnonymousMmap() {
  return mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
              MAP_ANONYMOUS | MAP_SHARED, -1, 0);
}

BPF_TEST(NaClNonSfiSandboxTest, mmap_allowed,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  void* ptr = DoAllowedAnonymousMmap();
  BPF_ASSERT_NE(MAP_FAILED, ptr);
  BPF_ASSERT_EQ(0, munmap(ptr, getpagesize()));
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, mmap_unallowed_flag,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
       MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, mmap_unallowed_prot,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  mmap(NULL, getpagesize(), PROT_READ | PROT_GROWSDOWN,
       MAP_ANONYMOUS, -1, 0);
}

// TODO(hamaji): Disallow RWX mmap.
#if 0
BPF_DEATH_TEST(NaClNonSfiSandboxTest, mmap_rwx,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC,
       MAP_ANONYMOUS, -1, 0);
}
#endif

BPF_TEST(NaClNonSfiSandboxTest, mprotect_allowed,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  void* ptr = DoAllowedAnonymousMmap();
  BPF_ASSERT_NE(MAP_FAILED, ptr);
  BPF_ASSERT_EQ(0, mprotect(ptr, getpagesize(), PROT_READ));
  BPF_ASSERT_EQ(0, munmap(ptr, getpagesize()));
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, mprotect_unallowed_prot,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  // We have tested DoAllowedAnonymousMmap is allowed in
  // mmap_allowed, so we can make sure the following mprotect call
  // kills the process.
  void* ptr = DoAllowedAnonymousMmap();
  BPF_ASSERT_NE(MAP_FAILED, ptr);
  mprotect(ptr, getpagesize(), PROT_READ | PROT_GROWSDOWN);
}

BPF_TEST(NaClNonSfiSandboxTest, brk,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  char* next_brk = static_cast<char*>(sbrk(0)) + getpagesize();
  // The kernel interface must return zero for brk.
  BPF_ASSERT_EQ(0, syscall(__NR_brk, next_brk));
  // The libc wrapper translates it to ENOMEM.
  errno = 0;
  BPF_ASSERT_EQ(-1, brk(next_brk));
  BPF_ASSERT_EQ(ENOMEM, errno);
}

#if defined(__i386__) || defined(__arm__)
BPF_TEST(NaClNonSfiSandboxTest, getegid32_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_getegid32));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST(NaClNonSfiSandboxTest, geteuid32_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_geteuid32));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST(NaClNonSfiSandboxTest, getgid32_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_getgid32));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST(NaClNonSfiSandboxTest, getuid32_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_getuid32));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, getegid_SIGSYS,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  syscall(__NR_getegid);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, geteuid_SIGSYS,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  syscall(__NR_geteuid);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, getgid_SIGSYS,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  syscall(__NR_getgid);
}

BPF_DEATH_TEST(NaClNonSfiSandboxTest, getuid_SIGSYS,
               DEATH_MESSAGE(sandbox::GetErrorMessageContentForTests()),
               nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  syscall(__NR_getuid);
}
#endif

#if defined(__x86_64__)
BPF_TEST(NaClNonSfiSandboxTest, getegid_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_getegid));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST(NaClNonSfiSandboxTest, geteuid_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_geteuid));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST(NaClNonSfiSandboxTest, getgid_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_getgid));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST(NaClNonSfiSandboxTest, getuid_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_getuid));
  BPF_ASSERT_EQ(EPERM, errno);
}
#endif

BPF_TEST(NaClNonSfiSandboxTest, getpid_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_getpid));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST(NaClNonSfiSandboxTest, ioctl_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_ioctl));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST(NaClNonSfiSandboxTest, readlink_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_readlink));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST(NaClNonSfiSandboxTest, madvise_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_madvise));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST(NaClNonSfiSandboxTest, open_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_open));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST(NaClNonSfiSandboxTest, ptrace_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_ptrace));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST(NaClNonSfiSandboxTest, set_robust_list_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_set_robust_list));
  BPF_ASSERT_EQ(EPERM, errno);
}

#if defined(__i386__) || defined(__x86_64__)
BPF_TEST(NaClNonSfiSandboxTest, time_EPERM,
         nacl::nonsfi::NaClNonSfiBPFSandboxPolicy::EvaluateSyscallImpl) {
  errno = 0;
  BPF_ASSERT_EQ(-1, syscall(__NR_time));
  BPF_ASSERT_EQ(EPERM, errno);
}
#endif

}  // namespace
