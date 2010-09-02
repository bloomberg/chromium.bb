// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_handler_host_linux.h"

#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/format_macros.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "breakpad/src/client/linux/handler/exception_handler.h"
#include "breakpad/src/client/linux/minidump_writer/linux_dumper.h"
#include "breakpad/src/client/linux/minidump_writer/minidump_writer.h"
#include "chrome/app/breakpad_linux.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/env_vars.h"

using google_breakpad::ExceptionHandler;

// Since classes derived from CrashHandlerHostLinux are singletons, it's only
// destroyed at the end of the processes lifetime, which is greater in span than
// the lifetime of the IO message loop.
DISABLE_RUNNABLE_METHOD_REFCOUNT(CrashHandlerHostLinux);

CrashHandlerHostLinux::CrashHandlerHostLinux()
    : process_socket_(-1),
      browser_socket_(-1) {
  int fds[2];
  // We use SOCK_SEQPACKET rather than SOCK_DGRAM to prevent the process from
  // sending datagrams to other sockets on the system. The sandbox may prevent
  // the process from calling socket() to create new sockets, but it'll still
  // inherit some sockets. With PF_UNIX+SOCK_DGRAM, it can call sendmsg to send
  // a datagram to any (abstract) socket on the same system. With
  // SOCK_SEQPACKET, this is prevented.
  CHECK_EQ(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds), 0);
  static const int on = 1;

  // Enable passcred on the server end of the socket
  CHECK_EQ(setsockopt(fds[1], SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)), 0);

  process_socket_ = fds[0];
  browser_socket_ = fds[1];

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &CrashHandlerHostLinux::Init));
}

CrashHandlerHostLinux::~CrashHandlerHostLinux() {
  HANDLE_EINTR(close(process_socket_));
  HANDLE_EINTR(close(browser_socket_));
}

void CrashHandlerHostLinux::Init() {
  MessageLoopForIO* ml = MessageLoopForIO::current();
  CHECK(ml->WatchFileDescriptor(
      browser_socket_, true /* persistent */,
      MessageLoopForIO::WATCH_READ,
      &file_descriptor_watcher_, this));
  ml->AddDestructionObserver(this);
}

void CrashHandlerHostLinux::OnFileCanWriteWithoutBlocking(int fd) {
  DCHECK(false);
}

void CrashHandlerHostLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(fd, browser_socket_);

  // A process has crashed and has signaled us by writing a datagram
  // to the death signal socket. The datagram contains the crash context needed
  // for writing the minidump as well as a file descriptor and a credentials
  // block so that they can't lie about their pid.

  // The length of the control message:
  static const unsigned kControlMsgSize =
      CMSG_SPACE(2*sizeof(int)) + CMSG_SPACE(sizeof(struct ucred));
  // The length of the regular payload:
  static const unsigned kCrashContextSize =
      sizeof(ExceptionHandler::CrashContext);

  struct msghdr msg = {0};
  struct iovec iov[6];
  char crash_context[kCrashContextSize];
  char guid[kGuidSize + 1];
  char crash_url[kMaxActiveURLSize + 1];
  char distro[kDistroSize + 1];
  char* tid_buf_addr = NULL;
  int tid_fd = -1;
  char control[kControlMsgSize];
  const ssize_t expected_msg_size = sizeof(crash_context) + sizeof(guid) +
      sizeof(crash_url) + sizeof(distro) +
      sizeof(tid_buf_addr) + sizeof(tid_fd);

  iov[0].iov_base = crash_context;
  iov[0].iov_len = sizeof(crash_context);
  iov[1].iov_base = guid;
  iov[1].iov_len = sizeof(guid);
  iov[2].iov_base = crash_url;
  iov[2].iov_len = sizeof(crash_url);
  iov[3].iov_base = distro;
  iov[3].iov_len = sizeof(distro);
  iov[4].iov_base = &tid_buf_addr;
  iov[4].iov_len = sizeof(tid_buf_addr);
  iov[5].iov_base = &tid_fd;
  iov[5].iov_len = sizeof(tid_fd);
  msg.msg_iov = iov;
  msg.msg_iovlen = 6;
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
  int partner_fd = -1;
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
      if (num_fds != 2) {
        // A nasty process could try and send us too many descriptors and
        // force a leak.
        LOG(ERROR) << "Death signal contained wrong number of descriptors;"
                   << " num_fds:" << num_fds;
        for (unsigned i = 0; i < num_fds; ++i)
          HANDLE_EINTR(close(reinterpret_cast<int*>(CMSG_DATA(hdr))[i]));
        return;
      } else {
        partner_fd = reinterpret_cast<int*>(CMSG_DATA(hdr))[0];
        signal_fd = reinterpret_cast<int*>(CMSG_DATA(hdr))[1];
      }
    } else if (hdr->cmsg_type == SCM_CREDENTIALS) {
      const struct ucred *cred =
          reinterpret_cast<struct ucred*>(CMSG_DATA(hdr));
      crashing_pid = cred->pid;
    }
  }

  if (crashing_pid == -1 || partner_fd == -1 || signal_fd == -1) {
    LOG(ERROR) << "Death signal message didn't contain all expected control"
               << " messages";
    if (partner_fd >= 0)
      HANDLE_EINTR(close(partner_fd));
    if (signal_fd >= 0)
      HANDLE_EINTR(close(signal_fd));
    return;
  }

  // Kernel bug workaround (broken in 2.6.30 at least):
  // The kernel doesn't translate PIDs in SCM_CREDENTIALS across PID
  // namespaces. Thus |crashing_pid| might be garbage from our point of view.
  // In the future we can remove this workaround, but we have to wait a couple
  // of years to be sure that it's worked its way out into the world.

  // The crashing process closes its copy of the signal_fd immediately after
  // calling sendmsg(). We can thus not reliably look for with with
  // FindProcessHoldingSocket(). But by necessity, it has to keep the
  // partner_fd open until the crashdump is complete.
  uint64_t inode_number;
  if (!base::FileDescriptorGetInode(&inode_number, partner_fd)) {
    LOG(WARNING) << "Failed to get inode number for passed socket";
    HANDLE_EINTR(close(partner_fd));
    HANDLE_EINTR(close(signal_fd));
    return;
  }
  HANDLE_EINTR(close(partner_fd));

  pid_t actual_crashing_pid = -1;
  if (!base::FindProcessHoldingSocket(&actual_crashing_pid, inode_number)) {
    LOG(WARNING) << "Failed to find process holding other end of crash reply "
                    "socket";
    HANDLE_EINTR(close(signal_fd));
    return;
  }

  if (actual_crashing_pid != crashing_pid) {
    crashing_pid = actual_crashing_pid;

    // The crashing TID set inside the compromised context via sys_gettid()
    // in ExceptionHandler::HandleSignal is also wrong and needs to be
    // translated.
    //
    // We expect the crashing thread to be in sys_read(), waiting for use to
    // write to |signal_fd|. Most newer kernels where we have the different pid
    // namespaces also have /proc/[pid]/syscall, so we can look through
    // |actual_crashing_pid|'s thread group and find the thread that's in the
    // read syscall with the right arguments.

    std::string expected_syscall_data;
    // /proc/[pid]/syscall is formatted as follows:
    // syscall_number arg1 ... arg6 sp pc
    // but we just check syscall_number through arg3.
    StringAppendF(&expected_syscall_data, "%d 0x%x %p 0x1 ",
                  SYS_read, tid_fd, tid_buf_addr);
    pid_t crashing_tid =
        base::FindThreadIDWithSyscall(crashing_pid, expected_syscall_data);
    if (crashing_tid == -1) {
      // We didn't find the thread we want. Maybe it didn't reach sys_read()
      // yet, or the kernel doesn't support /proc/[pid]/syscall or the thread
      // went away.  We'll just take a guess here and assume the crashing
      // thread is the thread group leader.
      crashing_tid = crashing_pid;
    }

    ExceptionHandler::CrashContext* bad_context =
        reinterpret_cast<ExceptionHandler::CrashContext*>(crash_context);
    bad_context->tid = crashing_tid;
  }

  bool upload = true;
  FilePath dumps_path("/tmp");
  if (getenv(env_vars::kHeadless)) {
    upload = false;
    PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path);
  }
  const uint64 rand = base::RandUint64();
  const std::string minidump_filename =
      StringPrintf("%s/chromium-%s-minidump-%016" PRIx64 ".dmp",
                   dumps_path.value().c_str(), process_type_.c_str(), rand);
  if (!google_breakpad::WriteMinidump(minidump_filename.c_str(),
                                      crashing_pid, crash_context,
                                      kCrashContextSize)) {
    LOG(ERROR) << "Failed to write crash dump for pid " << crashing_pid;
    HANDLE_EINTR(close(signal_fd));
  }

  // Send the done signal to the process: it can exit now.
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
  info.process_type = process_type_.c_str();
  info.process_type_length = process_type_.length();
  info.crash_url = crash_url;
  info.crash_url_length = strlen(crash_url);
  info.guid = guid;
  info.guid_length = strlen(guid);
  info.distro = distro;
  info.distro_length = strlen(distro);
  info.upload = upload;
  HandleCrashDump(info);
}

void CrashHandlerHostLinux::WillDestroyCurrentMessageLoop() {
  file_descriptor_watcher_.StopWatchingFileDescriptor();
}
