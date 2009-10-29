// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_crash_handler_host_linux.h"

#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/format_macros.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "breakpad/linux/exception_handler.h"
#include "breakpad/linux/linux_dumper.h"
#include "breakpad/linux/minidump_writer.h"
#include "chrome/app/breakpad_linux.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/chrome_paths.h"

// Since RenderCrashHandlerHostLinux is a singleton, it's only destroyed at the
// end of the processes lifetime, which is greater in span then the lifetime of
// the IO message loop.
template<> struct RunnableMethodTraits<RenderCrashHandlerHostLinux> {
  void RetainCallee(RenderCrashHandlerHostLinux*) { }
  void ReleaseCallee(RenderCrashHandlerHostLinux*) { }
};

RenderCrashHandlerHostLinux::RenderCrashHandlerHostLinux()
    : renderer_socket_(-1),
      browser_socket_(-1) {
  int fds[2];
  // We use SOCK_SEQPACKET rather than SOCK_DGRAM to prevent the renderer from
  // sending datagrams to other sockets on the system. The sandbox may prevent
  // the renderer from calling socket() to create new sockets, but it'll still
  // inherit some sockets. With PF_UNIX+SOCK_DGRAM, it can call sendmsg to send
  // a datagram to any (abstract) socket on the same system. With
  // SOCK_SEQPACKET, this is prevented.
  CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);
  static const int on = 1;

  // Enable passcred on the server end of the socket
  CHECK(setsockopt(fds[1], SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)) == 0);

  renderer_socket_ = fds[0];
  browser_socket_ = fds[1];

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &RenderCrashHandlerHostLinux::Init));
}

RenderCrashHandlerHostLinux::~RenderCrashHandlerHostLinux() {
  HANDLE_EINTR(close(renderer_socket_));
  HANDLE_EINTR(close(browser_socket_));
}

void RenderCrashHandlerHostLinux::Init() {
  MessageLoopForIO* ml = MessageLoopForIO::current();
  CHECK(ml->WatchFileDescriptor(
      browser_socket_, true /* persistent */,
      MessageLoopForIO::WATCH_READ,
      &file_descriptor_watcher_, this));
  ml->AddDestructionObserver(this);
}

void RenderCrashHandlerHostLinux::OnFileCanWriteWithoutBlocking(int fd) {
  DCHECK(false);
}

void RenderCrashHandlerHostLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(fd, browser_socket_);

  // A renderer process has crashed and has signaled us by writing a datagram
  // to the death signal socket. The datagram contains the crash context needed
  // for writing the minidump as well as a file descriptor and a credentials
  // block so that they can't lie about their pid.

  // The length of the control message:
  static const unsigned kControlMsgSize =
      CMSG_SPACE(sizeof(int)) + CMSG_SPACE(sizeof(struct ucred));
  // The length of the regular payload:
  static const unsigned kCrashContextSize =
      sizeof(google_breakpad::ExceptionHandler::CrashContext);

  struct msghdr msg = {0};
  struct iovec iov[4];
  char crash_context[kCrashContextSize];
  char guid[kGuidSize + 1];
  char crash_url[kMaxActiveURLSize + 1];
  char distro[kDistroSize + 1];
  char control[kControlMsgSize];
  const ssize_t expected_msg_size = sizeof(crash_context) + sizeof(guid) +
      sizeof(crash_url) + sizeof(distro);

  iov[0].iov_base = crash_context;
  iov[0].iov_len = sizeof(crash_context);
  iov[1].iov_base = guid;
  iov[1].iov_len = sizeof(guid);
  iov[2].iov_base = crash_url;
  iov[2].iov_len = sizeof(crash_url);
  iov[3].iov_base = distro;
  iov[3].iov_len = sizeof(distro);
  msg.msg_iov = iov;
  msg.msg_iovlen = 4;
  msg.msg_control = control;
  msg.msg_controllen = kControlMsgSize;

  const ssize_t msg_size = HANDLE_EINTR(recvmsg(browser_socket_, &msg, 0));
  if (msg_size != expected_msg_size) {
    LOG(ERROR) << "Error reading from death signal socket. Crash dumping"
               << " is disabled."
               << " msg_size:" << msg_size
               << " errno:" << errno;
    file_descriptor_watcher_.StopWatchingFileDescriptor();
    return;
  }

  if (msg.msg_controllen != kControlMsgSize ||
      msg.msg_flags & ~MSG_TRUNC) {
    LOG(ERROR) << "Received death signal message with the wrong size;"
               << " msg.msg_controllen:" << msg.msg_controllen
               << " msg.msg_flags:" << msg.msg_flags
               << " kCrashContextSize:" << kCrashContextSize
               << " kControlMsgSize:" << kControlMsgSize;
    return;
  }

  // Walk the control payload an extract the file descriptor and validated pid.
  pid_t crashing_pid = -1;
  int signal_fd = -1;
  for (struct cmsghdr *hdr = CMSG_FIRSTHDR(&msg); hdr;
       hdr = CMSG_NXTHDR(&msg, hdr)) {
    if (hdr->cmsg_level != SOL_SOCKET)
      continue;
    if (hdr->cmsg_type == SCM_RIGHTS) {
      const unsigned len = hdr->cmsg_len -
          (((uint8_t*)CMSG_DATA(hdr)) - (uint8_t*)hdr);
      DCHECK_EQ(len % sizeof(int), 0u);
      const unsigned num_fds = len / sizeof(int);
      if (num_fds > 1 || num_fds == 0) {
        // A nasty renderer could try and send us too many descriptors and
        // force a leak.
        LOG(ERROR) << "Death signal contained too many descriptors;"
                   << " num_fds:" << num_fds;
        for (unsigned i = 0; i < num_fds; ++i)
          HANDLE_EINTR(close(reinterpret_cast<int*>(CMSG_DATA(hdr))[i]));
        return;
      } else {
        signal_fd = reinterpret_cast<int*>(CMSG_DATA(hdr))[0];
      }
    } else if (hdr->cmsg_type == SCM_CREDENTIALS) {
      const struct ucred *cred =
          reinterpret_cast<struct ucred*>(CMSG_DATA(hdr));
      crashing_pid = cred->pid;
    }
  }

  if (crashing_pid == -1 || signal_fd == -1) {
    LOG(ERROR) << "Death signal message didn't contain all expected control"
               << " messages";
    if (signal_fd)
      HANDLE_EINTR(close(signal_fd));
    return;
  }

  // Kernel bug workaround (broken in 2.6.30 at least):
  // The kernel doesn't translate PIDs in SCM_CREDENTIALS across PID
  // namespaces. Thus |crashing_pid| might be garbage from our point of view.
  // In the future we can remove this workaround, but we have to wait a couple
  // of years to be sure that it's worked its way out into the world.

  uint64_t inode_number;
  if (!base::FileDescriptorGetInode(&inode_number, signal_fd)) {
    LOG(WARNING) << "Failed to get inode number for passed socket";
    HANDLE_EINTR(close(signal_fd));
    return;
  }

  if (!base::FindProcessHoldingSocket(&crashing_pid, inode_number - 1)) {
    LOG(WARNING) << "Failed to find process holding other end of crash reply "
                    "socket";
    HANDLE_EINTR(close(signal_fd));
    return;
  }

  bool upload = true;
  FilePath dumps_path("/tmp");
  if (getenv("CHROME_HEADLESS")) {
    upload = false;
    PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path);
  }
  const uint64 rand = base::RandUint64();
  const std::string minidump_filename =
      StringPrintf("%s/chromium-renderer-minidump-%016" PRIx64 ".dmp",
                   dumps_path.value().c_str(), rand);
  if (!google_breakpad::WriteMinidump(minidump_filename.c_str(),
                                      crashing_pid, crash_context,
                                      kCrashContextSize)) {
    LOG(ERROR) << "Failed to write crash dump for pid " << crashing_pid;
    HANDLE_EINTR(close(signal_fd));
  }

  // Send the done signal to the renderer: it can exit now.
  memset(&msg, 0, sizeof(msg));
  struct iovec done_iov;
  done_iov.iov_base = const_cast<char*>("\x42");
  done_iov.iov_len = 1;
  msg.msg_iov = &done_iov;
  msg.msg_iovlen = 1;

  HANDLE_EINTR(sendmsg(signal_fd, &msg, MSG_DONTWAIT | MSG_NOSIGNAL));
  HANDLE_EINTR(close(signal_fd));

  // Sanitize the string data a bit more
  guid[kGuidSize] = crash_url[kMaxActiveURLSize] = distro[kDistroSize] = 0;

  BreakpadInfo info;
  info.filename = minidump_filename.c_str();
  info.process_type = "renderer";
  info.process_type_length = 8;
  info.crash_url = crash_url;
  info.crash_url_length = strlen(crash_url);
  info.guid = guid;
  info.guid_length = strlen(guid);
  info.distro = distro;
  info.distro_length = strlen(distro);
  info.upload = upload;
  HandleCrashDump(info);
}

void RenderCrashHandlerHostLinux::WillDestroyCurrentMessageLoop() {
  file_descriptor_watcher_.StopWatchingFileDescriptor();
}
