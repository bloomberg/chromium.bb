// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/nacl_fork_delegate_linux.h"

#include <signal.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "content/common/unix_domain_socket_posix.h"
#include "content/common/zygote_fork_delegate_linux.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/nacl_helper_linux.h"

NaClForkDelegate::NaClForkDelegate()
    : ready_(false),
      sandboxed_(false),
      fd_(-1) {}

void NaClForkDelegate::Init(const bool sandboxed,
                            const int browserdesc,
                            const int sandboxdesc) {
  VLOG(1) << "NaClForkDelegate::Init()";
  int fds[2];

  sandboxed_ = sandboxed;
  // Confirm a couple hard-wired assumptions.
  // The NaCl constants are from chrome/nacl/nacl_linux_helper.h
  DCHECK(kNaClBrowserDescriptor == browserdesc);
  DCHECK(kNaClSandboxDescriptor == sandboxdesc);

  CHECK(socketpair(PF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);
  base::file_handle_mapping_vector fds_to_map;
  fds_to_map.push_back(std::make_pair(fds[1], kNaClZygoteDescriptor));
  fds_to_map.push_back(std::make_pair(sandboxdesc, kNaClSandboxDescriptor));
  // TODO(bradchen): To make this the default for release builds,
  // remove command line switch.
  ready_ = false;
  const bool use_helper = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kNaClLinuxHelper);
  FilePath helper_exe;
  if (use_helper && PathService::Get(chrome::FILE_NACL_HELPER, &helper_exe)) {
    CommandLine::StringVector argv = CommandLine::ForCurrentProcess()->argv();
    argv[0] = helper_exe.value();
    base::LaunchOptions options;
    options.fds_to_remap = &fds_to_map;
    options.clone_flags = CLONE_FS | SIGCHLD;
    // LD_BIND_NOW forces non-lazy binding in the dynamic linker, to
    // prevent the linker from trying to look at the text of the nacl_helper
    // program after it has been replaced by the nacl module.
    base::environment_vector env;
    env.push_back(std::make_pair("LD_BIND_NOW", "1"));
    options.environ = &env;
    ready_ = base::LaunchProcess(argv, options, NULL);
    // parent and error cases are handled below
  }
  if (HANDLE_EINTR(close(fds[1])) != 0)
    LOG(ERROR) << "close(fds[1]) failed";
  if (ready_) {
    const ssize_t kExpectedLength = strlen(kNaClHelperStartupAck);
    char buf[kExpectedLength];

    // Wait for ack from nacl_helper, indicating it is ready to help
    const ssize_t nread = HANDLE_EINTR(read(fds[0], buf, sizeof(buf)));
    if (nread == kExpectedLength &&
        memcmp(buf, kNaClHelperStartupAck, nread) == 0) {
      // all is well
      fd_ = fds[0];
      return;
    }
    LOG(ERROR) << "Bad NaCl helper startup ack (" << nread << " bytes)";
  }
  // TODO(bradchen): Make this LOG(ERROR) when the NaCl helper
  // becomes the default.
  ready_ = false;
  fd_ = -1;
  if (HANDLE_EINTR(close(fds[0])) != 0)
    LOG(ERROR) << "close(fds[0]) failed";
}

NaClForkDelegate::~NaClForkDelegate() {
  // side effect of close: delegate process will terminate
  if (ready_) {
    if (HANDLE_EINTR(close(fd_)) != 0)
      LOG(ERROR) << "close(fd_) failed";
  }
}

bool NaClForkDelegate::CanHelp(const std::string& process_type) {
  return (process_type == switches::kNaClLoaderProcess && ready_);
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
