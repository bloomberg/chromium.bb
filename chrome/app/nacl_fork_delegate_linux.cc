// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/nacl_fork_delegate_linux.h"

#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include <set>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/process_util.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/nacl_helper_linux.h"

NaClForkDelegate::NaClForkDelegate()
    : status_(kNaClHelperUnused),
      fd_(-1) {}

// Note these need to match up with their counterparts in nacl_helper_linux.c
// and nacl_helper_bootstrap_linux.c.
const char kNaClHelperReservedAtZero[] =
    "--reserved_at_zero=0xXXXXXXXXXXXXXXXX";
const char kNaClHelperRDebug[] = "--r_debug=0xXXXXXXXXXXXXXXXX";

void NaClForkDelegate::Init(const int sandboxdesc) {
  VLOG(1) << "NaClForkDelegate::Init()";
  int fds[2];

  // Confirm a hard-wired assumption.
  // The NaCl constant is from chrome/nacl/nacl_linux_helper.h
  DCHECK(kNaClSandboxDescriptor == sandboxdesc);

  CHECK(socketpair(PF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);
  base::FileHandleMappingVector fds_to_map;
  fds_to_map.push_back(std::make_pair(fds[1], kNaClZygoteDescriptor));
  fds_to_map.push_back(std::make_pair(sandboxdesc, kNaClSandboxDescriptor));

  status_ = kNaClHelperUnused;
  FilePath helper_exe;
  FilePath helper_bootstrap_exe;
  if (!PathService::Get(chrome::FILE_NACL_HELPER, &helper_exe)) {
    status_ = kNaClHelperMissing;
  } else if (!PathService::Get(chrome::FILE_NACL_HELPER_BOOTSTRAP,
                               &helper_bootstrap_exe)) {
    status_ = kNaClHelperBootstrapMissing;
  } else if (RunningOnValgrind()) {
    status_ = kNaClHelperValgrind;
  } else {
    CommandLine cmd_line(helper_bootstrap_exe);
    cmd_line.AppendArgPath(helper_exe);
    cmd_line.AppendArgNative(kNaClHelperReservedAtZero);
    cmd_line.AppendArgNative(kNaClHelperRDebug);
    base::LaunchOptions options;
    options.fds_to_remap = &fds_to_map;
    options.clone_flags = CLONE_FS | SIGCHLD;

    // The NaCl processes spawned may need to exceed the ambient soft limit
    // on RLIMIT_AS to allocate the untrusted address space and its guard
    // regions.  The nacl_helper itself cannot just raise its own limit,
    // because the existing limit may prevent the initial exec of
    // nacl_helper_bootstrap from succeeding, with its large address space
    // reservation.
    std::set<int> max_these_limits;
    max_these_limits.insert(RLIMIT_AS);
    options.maximize_rlimits = &max_these_limits;

    if (!base::LaunchProcess(cmd_line.argv(), options, NULL))
      status_ = kNaClHelperLaunchFailed;
    // parent and error cases are handled below
  }
  if (HANDLE_EINTR(close(fds[1])) != 0)
    LOG(ERROR) << "close(fds[1]) failed";
  if (status_ == kNaClHelperUnused) {
    const ssize_t kExpectedLength = strlen(kNaClHelperStartupAck);
    char buf[kExpectedLength];

    // Wait for ack from nacl_helper, indicating it is ready to help
    const ssize_t nread = HANDLE_EINTR(read(fds[0], buf, sizeof(buf)));
    if (nread == kExpectedLength &&
        memcmp(buf, kNaClHelperStartupAck, nread) == 0) {
      // all is well
      status_ = kNaClHelperSuccess;
      fd_ = fds[0];
      return;
    }

    status_ = kNaClHelperAckFailed;
    LOG(ERROR) << "Bad NaCl helper startup ack (" << nread << " bytes)";
  }
  // TODO(bradchen): Make this LOG(ERROR) when the NaCl helper
  // becomes the default.
  fd_ = -1;
  if (HANDLE_EINTR(close(fds[0])) != 0)
    LOG(ERROR) << "close(fds[0]) failed";
}

void NaClForkDelegate::InitialUMA(std::string* uma_name,
                                  int* uma_sample,
                                  int* uma_boundary_value) {
  *uma_name = "NaCl.Client.Helper.InitState";
  *uma_sample = status_;
  *uma_boundary_value = kNaClHelperStatusBoundary;
}

NaClForkDelegate::~NaClForkDelegate() {
  // side effect of close: delegate process will terminate
  if (status_ == kNaClHelperSuccess) {
    if (HANDLE_EINTR(close(fd_)) != 0)
      LOG(ERROR) << "close(fd_) failed";
  }
}

bool NaClForkDelegate::CanHelp(const std::string& process_type,
                               std::string* uma_name,
                               int* uma_sample,
                               int* uma_boundary_value) {
  if (process_type != switches::kNaClLoaderProcess)
    return false;
  *uma_name = "NaCl.Client.Helper.StateOnFork";
  *uma_sample = status_;
  *uma_boundary_value = kNaClHelperStatusBoundary;
  return status_ == kNaClHelperSuccess;
}

pid_t NaClForkDelegate::Fork(const std::vector<int>& fds) {
  base::ProcessId naclchild;
  VLOG(1) << "NaClForkDelegate::Fork";

  DCHECK(fds.size() == kNaClParentFDIndex + 1);
  if (!UnixDomainSocket::SendMsg(fd_, kNaClForkRequest,
                                 strlen(kNaClForkRequest), fds)) {
    LOG(ERROR) << "NaClForkDelegate::Fork: SendMsg failed";
    return -1;
  }
  int nread = HANDLE_EINTR(read(fd_, &naclchild, sizeof(naclchild)));
  if (nread != sizeof(naclchild)) {
    LOG(ERROR) << "NaClForkDelegate::Fork: read failed";
    return -1;
  }
  VLOG(1) << "nacl_child is " << naclchild << " (" << nread << " bytes)";
  return naclchild;
}

bool NaClForkDelegate::AckChild(const int fd,
                                const std::string& channel_switch) {
  int nwritten = HANDLE_EINTR(write(fd, channel_switch.c_str(),
                                    channel_switch.length()));
  if (nwritten != static_cast<int>(channel_switch.length())) {
    return false;
  }
  return true;
}
