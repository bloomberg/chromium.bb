// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <string>
#include <map>

#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/global_descriptors_posix.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_switches.h"
#include "ipc/file_descriptor_set_posix.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_utils.h"

namespace IPC {

// IPC channels on Windows use named pipes (CreateNamedPipe()) with
// channel ids as the pipe names.  Channels on POSIX use sockets as
// pipes  These don't quite line up.
//
// When creating a child subprocess we use a socket pair and the parent side of
// the fork arranges it such that the initial control channel ends up on the
// magic file descriptor kPrimaryIPCChannel in the child.  Future
// connections (file descriptors) can then be passed via that
// connection via sendmsg().
//
// A POSIX IPC channel can also be set up as a server for a bound UNIX domain
// socket, and will handle multiple connect and disconnect sequences.  Currently
// it is limited to one connection at a time.

//------------------------------------------------------------------------------
namespace {

// The PipeMap class works around this quirk related to unit tests:
//
// When running as a server, we install the client socket in a
// specific file descriptor number (@kPrimaryIPCChannel). However, we
// also have to support the case where we are running unittests in the
// same process.  (We do not support forking without execing.)
//
// Case 1: normal running
//   The IPC server object will install a mapping in PipeMap from the
//   name which it was given to the client pipe. When forking the client, the
//   GetClientFileDescriptorMapping will ensure that the socket is installed in
//   the magic slot (@kPrimaryIPCChannel). The client will search for the
//   mapping, but it won't find any since we are in a new process. Thus the
//   magic fd number is returned. Once the client connects, the server will
//   close its copy of the client socket and remove the mapping.
//
// Case 2: unittests - client and server in the same process
//   The IPC server will install a mapping as before. The client will search
//   for a mapping and find out. It duplicates the file descriptor and
//   connects. Once the client connects, the server will close the original
//   copy of the client socket and remove the mapping. Thus, when the client
//   object closes, it will close the only remaining copy of the client socket
//   in the fd table and the server will see EOF on its side.
//
// TODO(port): a client process cannot connect to multiple IPC channels with
// this scheme.

class PipeMap {
 public:
  static PipeMap* GetInstance() {
    return Singleton<PipeMap>::get();
  }

  ~PipeMap() {
    // Shouldn't have left over pipes.
    DCHECK(map_.empty());
  }

  // Lookup a given channel id. Return -1 if not found.
  int Lookup(const std::string& channel_id) {
    base::AutoLock locked(lock_);

    ChannelToFDMap::const_iterator i = map_.find(channel_id);
    if (i == map_.end())
      return -1;
    return i->second;
  }

  // Remove the mapping for the given channel id. No error is signaled if the
  // channel_id doesn't exist
  void RemoveAndClose(const std::string& channel_id) {
    base::AutoLock locked(lock_);

    ChannelToFDMap::iterator i = map_.find(channel_id);
    if (i != map_.end()) {
      if (HANDLE_EINTR(close(i->second)) < 0)
        PLOG(ERROR) << "close " << channel_id;
      map_.erase(i);
    }
  }

  // Insert a mapping from @channel_id to @fd. It's a fatal error to insert a
  // mapping if one already exists for the given channel_id
  void Insert(const std::string& channel_id, int fd) {
    base::AutoLock locked(lock_);
    DCHECK(fd != -1);

    ChannelToFDMap::const_iterator i = map_.find(channel_id);
    CHECK(i == map_.end()) << "Creating second IPC server (fd " << fd << ") "
                           << "for '" << channel_id << "' while first "
                           << "(fd " << i->second << ") still exists";
    map_[channel_id] = fd;
  }

 private:
  base::Lock lock_;
  typedef std::map<std::string, int> ChannelToFDMap;
  ChannelToFDMap map_;

  friend struct DefaultSingletonTraits<PipeMap>;
};

//------------------------------------------------------------------------------
// Verify that kMaxPipeNameLength is a decent size.
COMPILE_ASSERT(sizeof(((sockaddr_un*)0)->sun_path) >= kMaxPipeNameLength,
               BAD_SUN_PATH_LENGTH);

// Creates a unix domain socket bound to the specified name that is listening
// for connections.
bool CreateServerUnixDomainSocket(const std::string& pipe_name,
                                  int* server_listen_fd) {
  DCHECK(server_listen_fd);
  DCHECK_GT(pipe_name.length(), 0u);
  DCHECK_LT(pipe_name.length(), kMaxPipeNameLength);

  if (pipe_name.length() == 0 || pipe_name.length() >= kMaxPipeNameLength) {
    return false;
  }

  // Create socket.
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return false;
  }

  // Make socket non-blocking
  if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
    PLOG(ERROR) << "fcntl(O_NONBLOCK) " << pipe_name;
    if (HANDLE_EINTR(close(fd)) < 0)
      PLOG(ERROR) << "close " << pipe_name;
    return false;
  }

  // Delete any old FS instances.
  unlink(pipe_name.c_str());

  // Make sure the path we need exists.
  FilePath path(pipe_name);
  FilePath dir_path = path.DirName();
  if (!file_util::CreateDirectory(dir_path)) {
    return false;
  }

  // Create unix_addr structure.
  struct sockaddr_un unix_addr;
  memset(&unix_addr, 0, sizeof(unix_addr));
  unix_addr.sun_family = AF_UNIX;
  int path_len = snprintf(unix_addr.sun_path, IPC::kMaxPipeNameLength,
                          "%s", pipe_name.c_str());
  DCHECK_EQ(static_cast<int>(pipe_name.length()), path_len);
  size_t unix_addr_len = offsetof(struct sockaddr_un,
                                       sun_path) + path_len + 1;

  // Bind the socket.
  if (bind(fd, reinterpret_cast<const sockaddr*>(&unix_addr),
           unix_addr_len) != 0) {
    PLOG(ERROR) << "bind " << pipe_name;
    if (HANDLE_EINTR(close(fd)) < 0)
      PLOG(ERROR) << "close " << pipe_name;
    return false;
  }

  // Explicitly set file system permissions on socket, mainly as a precaution
  // for Chrome OS.
  // Do not rely on these file permissions to provide security - the file is
  // created during the above bind() call so there is still a window for
  // malicious abuse because the file exists between bind() and chmod(). Also,
  // the file permissions may not be enforced for unix sockets on all platforms.
  if (chmod(pipe_name.c_str(), 0600)) {
    PLOG(ERROR) << "chmod " << pipe_name;
    if (HANDLE_EINTR(close(fd)) < 0)
      PLOG(ERROR) << "close " << pipe_name;
    return false;
  }

  // Start listening on the socket.
  const int listen_queue_length = 1;
  if (listen(fd, listen_queue_length) != 0) {
    PLOG(ERROR) << "listen " << pipe_name;
    if (HANDLE_EINTR(close(fd)) < 0)
      PLOG(ERROR) << "close " << pipe_name;
    return false;
  }

  *server_listen_fd = fd;
  return true;
}

// Accept a connection on a socket we are listening to.
bool ServerAcceptConnection(int server_listen_fd, int* server_socket) {
  DCHECK(server_socket);

  int accept_fd = HANDLE_EINTR(accept(server_listen_fd, NULL, 0));
  if (accept_fd < 0)
    return false;
  if (fcntl(accept_fd, F_SETFL, O_NONBLOCK) == -1) {
    PLOG(ERROR) << "fcntl(O_NONBLOCK) " << accept_fd;
    if (HANDLE_EINTR(close(accept_fd)) < 0)
      PLOG(ERROR) << "close " << accept_fd;
    return false;
  }

  *server_socket = accept_fd;
  return true;
}

bool CreateClientUnixDomainSocket(const std::string& pipe_name,
                                  int* client_socket) {
  DCHECK(client_socket);
  DCHECK_GT(pipe_name.length(), 0u);
  DCHECK_LT(pipe_name.length(), kMaxPipeNameLength);

  if (pipe_name.length() == 0 || pipe_name.length() >= kMaxPipeNameLength) {
    return false;
  }

  // Create socket.
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    PLOG(ERROR) << "socket " << pipe_name;
    return false;
  }

  // Make socket non-blocking
  if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
    PLOG(ERROR) << "fcntl(O_NONBLOCK) " << pipe_name;
    if (HANDLE_EINTR(close(fd)) < 0)
      PLOG(ERROR) << "close " << pipe_name;
    return false;
  }

  // Create server side of socket.
  struct sockaddr_un server_unix_addr;
  memset(&server_unix_addr, 0, sizeof(server_unix_addr));
  server_unix_addr.sun_family = AF_UNIX;
  int path_len = snprintf(server_unix_addr.sun_path, IPC::kMaxPipeNameLength,
                          "%s", pipe_name.c_str());
  DCHECK_EQ(static_cast<int>(pipe_name.length()), path_len);
  size_t server_unix_addr_len = offsetof(struct sockaddr_un,
                                         sun_path) + path_len + 1;

  if (HANDLE_EINTR(connect(fd, reinterpret_cast<sockaddr*>(&server_unix_addr),
                           server_unix_addr_len)) != 0) {
    PLOG(ERROR) << "connect " << pipe_name;
    if (HANDLE_EINTR(close(fd)) < 0)
      PLOG(ERROR) << "close " << pipe_name;
    return false;
  }

  *client_socket = fd;
  return true;
}

bool SocketWriteErrorIsRecoverable() {
#if defined(OS_MACOSX)
  // On OS X if sendmsg() is trying to send fds between processes and there
  // isn't enough room in the output buffer to send the fd structure over
  // atomically then EMSGSIZE is returned.
  //
  // EMSGSIZE presents a problem since the system APIs can only call us when
  // there's room in the socket buffer and not when there is "enough" room.
  //
  // The current behavior is to return to the event loop when EMSGSIZE is
  // received and hopefull service another FD.  This is however still
  // technically a busy wait since the event loop will call us right back until
  // the receiver has read enough data to allow passing the FD over atomically.
  return errno == EAGAIN || errno == EMSGSIZE;
#else
  return errno == EAGAIN;
#endif  // OS_MACOSX
}

}  // namespace
//------------------------------------------------------------------------------

Channel::ChannelImpl::ChannelImpl(const IPC::ChannelHandle& channel_handle,
                                  Mode mode, Listener* listener)
    : mode_(mode),
      is_blocked_on_write_(false),
      waiting_connect_(true),
      message_send_bytes_written_(0),
      server_listen_pipe_(-1),
      pipe_(-1),
      client_pipe_(-1),
#if defined(IPC_USES_READWRITE)
      fd_pipe_(-1),
      remote_fd_pipe_(-1),
#endif  // IPC_USES_READWRITE
      pipe_name_(channel_handle.name),
      listener_(listener),
      must_unlink_(false),
      factory_(this) {
  if (!CreatePipe(channel_handle)) {
    // The pipe may have been closed already.
    const char *modestr = (mode_ & MODE_SERVER_FLAG) ? "server" : "client";
    // The pipe may have been closed already.
    LOG(WARNING) << "Unable to create pipe named \"" << channel_handle.name
                 << "\" in " << modestr << " mode";
  }
}

Channel::ChannelImpl::~ChannelImpl() {
  Close();
}

bool SocketPair(int* fd1, int* fd2) {
  int pipe_fds[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_fds) != 0) {
    PLOG(ERROR) << "socketpair()";
    return false;
  }

  // Set both ends to be non-blocking.
  if (fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK) == -1 ||
      fcntl(pipe_fds[1], F_SETFL, O_NONBLOCK) == -1) {
    PLOG(ERROR) << "fcntl(O_NONBLOCK)";
    if (HANDLE_EINTR(close(pipe_fds[0])) < 0)
      PLOG(ERROR) << "close";
    if (HANDLE_EINTR(close(pipe_fds[1])) < 0)
      PLOG(ERROR) << "close";
    return false;
  }

  *fd1 = pipe_fds[0];
  *fd2 = pipe_fds[1];

  return true;
}

bool Channel::ChannelImpl::CreatePipe(
    const IPC::ChannelHandle& channel_handle) {
  DCHECK(server_listen_pipe_ == -1 && pipe_ == -1);

  // Four possible cases:
  // 1) It's a channel wrapping a pipe that is given to us.
  // 2) It's for a named channel, so we create it.
  // 3) It's for a client that we implement ourself. This is used
  //    in unittesting.
  // 4) It's the initial IPC channel:
  //   4a) Client side: Pull the pipe out of the GlobalDescriptors set.
  //   4b) Server side: create the pipe.

  if (channel_handle.socket.fd != -1) {
    // Case 1 from comment above.
    pipe_ = channel_handle.socket.fd;
#if defined(IPC_USES_READWRITE)
    // Test the socket passed into us to make sure it is nonblocking.
    // We don't want to call read/write on a blocking socket.
    int value = fcntl(pipe_, F_GETFL);
    if (value == -1) {
      PLOG(ERROR) << "fcntl(F_GETFL) " << pipe_name_;
      return false;
    }
    if (!(value & O_NONBLOCK)) {
      LOG(ERROR) << "Socket " << pipe_name_ << " must be O_NONBLOCK";
      return false;
    }
#endif   // IPC_USES_READWRITE
  } else if (mode_ & MODE_NAMED_FLAG) {
    // Case 2 from comment above.
    if (mode_ & MODE_SERVER_FLAG) {
      if (!CreateServerUnixDomainSocket(pipe_name_, &pipe_)) {
        return false;
      }
      must_unlink_ = true;
    } else if (mode_ & MODE_CLIENT_FLAG) {
      if (!CreateClientUnixDomainSocket(pipe_name_, &pipe_)) {
        return false;
      }
    } else {
      NOTREACHED();
      return false;
    }
  } else {
    pipe_ = PipeMap::GetInstance()->Lookup(pipe_name_);
    if (mode_ & MODE_CLIENT_FLAG) {
      if (pipe_ != -1) {
        // Case 3 from comment above.
        // We only allow one connection.
        pipe_ = HANDLE_EINTR(dup(pipe_));
        PipeMap::GetInstance()->RemoveAndClose(pipe_name_);
      } else {
        // Case 4a from comment above.
        // Guard against inappropriate reuse of the initial IPC channel.  If
        // an IPC channel closes and someone attempts to reuse it by name, the
        // initial channel must not be recycled here.  http://crbug.com/26754.
        static bool used_initial_channel = false;
        if (used_initial_channel) {
          LOG(FATAL) << "Denying attempt to reuse initial IPC channel for "
                     << pipe_name_;
          return false;
        }
        used_initial_channel = true;

        pipe_ = base::GlobalDescriptors::GetInstance()->Get(kPrimaryIPCChannel);
      }
    } else if (mode_ & MODE_SERVER_FLAG) {
      // Case 4b from comment above.
      if (pipe_ != -1) {
        LOG(ERROR) << "Server already exists for " << pipe_name_;
        return false;
      }
      if (!SocketPair(&pipe_, &client_pipe_))
        return false;
      PipeMap::GetInstance()->Insert(pipe_name_, client_pipe_);
    } else {
      NOTREACHED();
      return false;
    }
  }

  if ((mode_ & MODE_SERVER_FLAG) && (mode_ & MODE_NAMED_FLAG)) {
    server_listen_pipe_ = pipe_;
    pipe_ = -1;
  }

#if defined(IPC_USES_READWRITE)
  // Create a dedicated socketpair() for exchanging file descriptors.
  // See comments for IPC_USES_READWRITE for details.
  if (mode_ & MODE_CLIENT_FLAG) {
    if (!SocketPair(&fd_pipe_, &remote_fd_pipe_)) {
      return false;
    }
  }
#endif  // IPC_USES_READWRITE

  return true;
}

bool Channel::ChannelImpl::Connect() {
  if (server_listen_pipe_ == -1 && pipe_ == -1) {
    DLOG(INFO) << "Must call create on a channel before calling connect";
    return false;
  }

  bool did_connect = true;
  if (server_listen_pipe_ != -1) {
    // Watch the pipe for connections, and turn any connections into
    // active sockets.
    MessageLoopForIO::current()->WatchFileDescriptor(
        server_listen_pipe_,
        true,
        MessageLoopForIO::WATCH_READ,
        &server_listen_connection_watcher_,
        this);
  } else {
    did_connect = AcceptConnection();
  }
  return did_connect;
}

bool Channel::ChannelImpl::ProcessIncomingMessages() {
  ssize_t bytes_read = 0;

  struct msghdr msg = {0};
  struct iovec iov = {input_buf_, Channel::kReadBufferSize};

  msg.msg_iovlen = 1;
  msg.msg_control = input_cmsg_buf_;

  for (;;) {
    msg.msg_iov = &iov;

    if (bytes_read == 0) {
      if (pipe_ == -1)
        return false;

      // Read from pipe.
      // recvmsg() returns 0 if the connection has closed or EAGAIN if no data
      // is waiting on the pipe.
#if defined(IPC_USES_READWRITE)
      if (fd_pipe_ >= 0) {
        bytes_read = HANDLE_EINTR(read(pipe_, input_buf_,
                                       Channel::kReadBufferSize));
        msg.msg_controllen = 0;
      } else
#endif  // IPC_USES_READWRITE
      {
        msg.msg_controllen = sizeof(input_cmsg_buf_);
        bytes_read = HANDLE_EINTR(recvmsg(pipe_, &msg, MSG_DONTWAIT));
      }
      if (bytes_read < 0) {
        if (errno == EAGAIN) {
          return true;
#if defined(OS_MACOSX)
        } else if (errno == EPERM) {
          // On OSX, reading from a pipe with no listener returns EPERM
          // treat this as a special case to prevent spurious error messages
          // to the console.
          return false;
#endif  // OS_MACOSX
        } else if (errno == ECONNRESET || errno == EPIPE) {
          return false;
        } else {
          PLOG(ERROR) << "pipe error (" << pipe_ << ")";
          return false;
        }
      } else if (bytes_read == 0) {
        // The pipe has closed...
        return false;
      }
    }
    DCHECK(bytes_read);

    if (client_pipe_ != -1) {
      PipeMap::GetInstance()->RemoveAndClose(pipe_name_);
      client_pipe_ = -1;
    }

    // a pointer to an array of |num_wire_fds| file descriptors from the read
    const int* wire_fds = NULL;
    unsigned num_wire_fds = 0;

    // walk the list of control messages and, if we find an array of file
    // descriptors, save a pointer to the array

    // This next if statement is to work around an OSX issue where
    // CMSG_FIRSTHDR will return non-NULL in the case that controllen == 0.
    // Here's a test case:
    //
    // int main() {
    // struct msghdr msg;
    //   msg.msg_control = &msg;
    //   msg.msg_controllen = 0;
    //   if (CMSG_FIRSTHDR(&msg))
    //     printf("Bug found!\n");
    // }
    if (msg.msg_controllen > 0) {
      // On OSX, CMSG_FIRSTHDR doesn't handle the case where controllen is 0
      // and will return a pointer into nowhere.
      for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg;
           cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET &&
            cmsg->cmsg_type == SCM_RIGHTS) {
          const unsigned payload_len = cmsg->cmsg_len - CMSG_LEN(0);
          DCHECK(payload_len % sizeof(int) == 0);
          wire_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
          num_wire_fds = payload_len / 4;

          if (msg.msg_flags & MSG_CTRUNC) {
            LOG(ERROR) << "SCM_RIGHTS message was truncated"
                       << " cmsg_len:" << cmsg->cmsg_len
                       << " fd:" << pipe_;
            for (unsigned i = 0; i < num_wire_fds; ++i)
              if (HANDLE_EINTR(close(wire_fds[i])) < 0)
                PLOG(ERROR) << "close " << i;
            return false;
          }
          break;
        }
      }
    }

    // Process messages from input buffer.
    const char *p;
    const char *end;
    if (input_overflow_buf_.empty()) {
      p = input_buf_;
      end = p + bytes_read;
    } else {
      if (input_overflow_buf_.size() >
         static_cast<size_t>(kMaximumMessageSize - bytes_read)) {
        input_overflow_buf_.clear();
        LOG(ERROR) << "IPC message is too big";
        return false;
      }
      input_overflow_buf_.append(input_buf_, bytes_read);
      p = input_overflow_buf_.data();
      end = p + input_overflow_buf_.size();
    }

    // A pointer to an array of |num_fds| file descriptors which includes any
    // fds that have spilled over from a previous read.
    const int* fds = NULL;
    unsigned num_fds = 0;
    unsigned fds_i = 0;  // the index of the first unused descriptor

    if (input_overflow_fds_.empty()) {
      fds = wire_fds;
      num_fds = num_wire_fds;
    } else {
      if (num_wire_fds > 0) {
        const size_t prev_size = input_overflow_fds_.size();
        input_overflow_fds_.resize(prev_size + num_wire_fds);
        memcpy(&input_overflow_fds_[prev_size], wire_fds,
               num_wire_fds * sizeof(int));
      }
      fds = &input_overflow_fds_[0];
      num_fds = input_overflow_fds_.size();
    }

    while (p < end) {
      const char* message_tail = Message::FindNext(p, end);
      if (message_tail) {
        int len = static_cast<int>(message_tail - p);
        Message m(p, len);
        const uint16 header_fds = m.header()->num_fds;
        if (header_fds) {
          // the message has file descriptors
          const char* error = NULL;
          if (header_fds > num_fds - fds_i) {
            // the message has been completely received, but we didn't get
            // enough file descriptors.
#if defined(IPC_USES_READWRITE)
            char dummy;
            struct iovec fd_pipe_iov = { &dummy, 1 };
            msg.msg_iov = &fd_pipe_iov;
            msg.msg_controllen = sizeof(input_cmsg_buf_);
            ssize_t n = HANDLE_EINTR(recvmsg(fd_pipe_, &msg, MSG_DONTWAIT));
            if (n == 1 && msg.msg_controllen > 0) {
              for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg;
                   cmsg = CMSG_NXTHDR(&msg, cmsg)) {
                if (cmsg->cmsg_level == SOL_SOCKET &&
                    cmsg->cmsg_type == SCM_RIGHTS) {
                  const unsigned payload_len = cmsg->cmsg_len - CMSG_LEN(0);
                  DCHECK(payload_len % sizeof(int) == 0);
                  wire_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
                  num_wire_fds = payload_len / 4;

                  if (msg.msg_flags & MSG_CTRUNC) {
                    LOG(ERROR) << "SCM_RIGHTS message was truncated"
                               << " cmsg_len:" << cmsg->cmsg_len
                               << " fd:" << pipe_;
                    for (unsigned i = 0; i < num_wire_fds; ++i)
                      if (HANDLE_EINTR(close(wire_fds[i])) < 0)
                        PLOG(ERROR) << "close " << i;
                    return false;
                  }
                  break;
                }
              }
              if (input_overflow_fds_.empty()) {
                fds = wire_fds;
                num_fds = num_wire_fds;
              } else {
                if (num_wire_fds > 0) {
                  const size_t prev_size = input_overflow_fds_.size();
                  input_overflow_fds_.resize(prev_size + num_wire_fds);
                  memcpy(&input_overflow_fds_[prev_size], wire_fds,
                         num_wire_fds * sizeof(int));
                }
                fds = &input_overflow_fds_[0];
                num_fds = input_overflow_fds_.size();
              }
            }
            if (header_fds > num_fds - fds_i)
#endif  // IPC_USES_READWRITE
              error = "Message needs unreceived descriptors";
          }

          if (header_fds >
              FileDescriptorSet::MAX_DESCRIPTORS_PER_MESSAGE) {
            // There are too many descriptors in this message
            error = "Message requires an excessive number of descriptors";
          }

          if (error) {
            LOG(WARNING) << error
                         << " channel:" << this
                         << " message-type:" << m.type()
                         << " header()->num_fds:" << header_fds
                         << " num_fds:" << num_fds
                         << " fds_i:" << fds_i;
#if defined(CHROMIUM_SELINUX)
            LOG(WARNING) << "In the case of SELinux this can be caused when "
                            "using a --user-data-dir to which the default "
                            "policy doesn't give the renderer access to. ";
#endif  // CHROMIUM_SELINUX
            // close the existing file descriptors so that we don't leak them
            for (unsigned i = fds_i; i < num_fds; ++i)
              if (HANDLE_EINTR(close(fds[i])) < 0)
                PLOG(ERROR) << "close " << i;
            input_overflow_fds_.clear();
            // abort the connection
            return false;
          }

          m.file_descriptor_set()->SetDescriptors(
              &fds[fds_i], header_fds);
          fds_i += header_fds;
        }
        DVLOG(2) << "received message on channel @" << this
                 << " with type " << m.type() << " on fd " << pipe_;
        if (IsHelloMessage(&m)) {
          // The Hello message contains only the process id.
          void *iter = NULL;
          int pid;
          if (!m.ReadInt(&iter, &pid)) {
            NOTREACHED();
          }
#if defined(IPC_USES_READWRITE)
          if (mode_ & MODE_SERVER_FLAG) {
            // With IPC_USES_READWRITE, the Hello message from the client to the
            // server also contains the fd_pipe_, which  will be used for all
            // subsequent file descriptor passing.
            DCHECK_EQ(m.file_descriptor_set()->size(), 1U);
            base::FileDescriptor descriptor;
            if (!m.ReadFileDescriptor(&iter, &descriptor)) {
              NOTREACHED();
            }
            fd_pipe_ = descriptor.fd;
            CHECK(descriptor.auto_close);
          }
#endif  // IPC_USES_READWRITE
          listener_->OnChannelConnected(pid);
        } else {
          listener_->OnMessageReceived(m);
        }
        p = message_tail;
      } else {
        // Last message is partial.
        break;
      }
      input_overflow_fds_ = std::vector<int>(&fds[fds_i], &fds[num_fds]);
      fds_i = 0;
      fds = &input_overflow_fds_[0];
      num_fds = input_overflow_fds_.size();
    }
    input_overflow_buf_.assign(p, end - p);
    input_overflow_fds_ = std::vector<int>(&fds[fds_i], &fds[num_fds]);

    // When the input data buffer is empty, the overflow fds should be too. If
    // this is not the case, we probably have a rogue renderer which is trying
    // to fill our descriptor table.
    if (input_overflow_buf_.empty() && !input_overflow_fds_.empty()) {
      // We close these descriptors in Close()
      return false;
    }

    bytes_read = 0;  // Get more data.
  }
}

bool Channel::ChannelImpl::ProcessOutgoingMessages() {
  DCHECK(!waiting_connect_);  // Why are we trying to send messages if there's
                              // no connection?
  if (output_queue_.empty())
    return true;

  if (pipe_ == -1)
    return false;

  // Write out all the messages we can till the write blocks or there are no
  // more outgoing messages.
  while (!output_queue_.empty()) {
    Message* msg = output_queue_.front();

    size_t amt_to_write = msg->size() - message_send_bytes_written_;
    DCHECK(amt_to_write != 0);
    const char* out_bytes = reinterpret_cast<const char*>(msg->data()) +
        message_send_bytes_written_;

    struct msghdr msgh = {0};
    struct iovec iov = {const_cast<char*>(out_bytes), amt_to_write};
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    char buf[CMSG_SPACE(
        sizeof(int[FileDescriptorSet::MAX_DESCRIPTORS_PER_MESSAGE]))];

    ssize_t bytes_written = 1;
    int fd_written = -1;

    if (message_send_bytes_written_ == 0 &&
        !msg->file_descriptor_set()->empty()) {
      // This is the first chunk of a message which has descriptors to send
      struct cmsghdr *cmsg;
      const unsigned num_fds = msg->file_descriptor_set()->size();

      DCHECK_LE(num_fds, FileDescriptorSet::MAX_DESCRIPTORS_PER_MESSAGE);
      if (msg->file_descriptor_set()->ContainsDirectoryDescriptor()) {
        LOG(FATAL) << "Panic: attempting to transport directory descriptor over"
                      " IPC. Aborting to maintain sandbox isolation.";
        // If you have hit this then something tried to send a file descriptor
        // to a directory over an IPC channel. Since IPC channels span
        // sandboxes this is very bad: the receiving process can use openat
        // with ".." elements in the path in order to reach the real
        // filesystem.
      }

      msgh.msg_control = buf;
      msgh.msg_controllen = CMSG_SPACE(sizeof(int) * num_fds);
      cmsg = CMSG_FIRSTHDR(&msgh);
      cmsg->cmsg_level = SOL_SOCKET;
      cmsg->cmsg_type = SCM_RIGHTS;
      cmsg->cmsg_len = CMSG_LEN(sizeof(int) * num_fds);
      msg->file_descriptor_set()->GetDescriptors(
          reinterpret_cast<int*>(CMSG_DATA(cmsg)));
      msgh.msg_controllen = cmsg->cmsg_len;

      // DCHECK_LE above already checks that
      // num_fds < MAX_DESCRIPTORS_PER_MESSAGE so no danger of overflow.
      msg->header()->num_fds = static_cast<uint16>(num_fds);

#if defined(IPC_USES_READWRITE)
      if (!IsHelloMessage(msg)) {
        // Only the Hello message sends the file descriptor with the message.
        // Subsequently, we can send file descriptors on the dedicated
        // fd_pipe_ which makes Seccomp sandbox operation more efficient.
        struct iovec fd_pipe_iov = { const_cast<char *>(""), 1 };
        msgh.msg_iov = &fd_pipe_iov;
        fd_written = fd_pipe_;
        bytes_written = HANDLE_EINTR(sendmsg(fd_pipe_, &msgh, MSG_DONTWAIT));
        msgh.msg_iov = &iov;
        msgh.msg_controllen = 0;
        if (bytes_written > 0) {
          msg->file_descriptor_set()->CommitAll();
        }
      }
#endif  // IPC_USES_READWRITE
    }

    if (bytes_written == 1) {
      fd_written = pipe_;
#if defined(IPC_USES_READWRITE)
      if ((mode_ & MODE_CLIENT_FLAG) && IsHelloMessage(msg)) {
        DCHECK_EQ(msg->file_descriptor_set()->size(), 1U);
      }
      if (!msgh.msg_controllen) {
        bytes_written = HANDLE_EINTR(write(pipe_, out_bytes, amt_to_write));
      } else
#endif  // IPC_USES_READWRITE
      {
        bytes_written = HANDLE_EINTR(sendmsg(pipe_, &msgh, MSG_DONTWAIT));
      }
    }
    if (bytes_written > 0)
      msg->file_descriptor_set()->CommitAll();

    if (bytes_written < 0 && !SocketWriteErrorIsRecoverable()) {
#if defined(OS_MACOSX)
      // On OSX writing to a pipe with no listener returns EPERM.
      if (errno == EPERM) {
        Close();
        return false;
      }
#endif  // OS_MACOSX
      if (errno == EPIPE) {
        Close();
        return false;
      }
      PLOG(ERROR) << "pipe error on "
                  << fd_written
                  << " Currently writing message of size: "
                  << msg->size();
      return false;
    }

    if (static_cast<size_t>(bytes_written) != amt_to_write) {
      if (bytes_written > 0) {
        // If write() fails with EAGAIN then bytes_written will be -1.
        message_send_bytes_written_ += bytes_written;
      }

      // Tell libevent to call us back once things are unblocked.
      is_blocked_on_write_ = true;
      MessageLoopForIO::current()->WatchFileDescriptor(
          pipe_,
          false,  // One shot
          MessageLoopForIO::WATCH_WRITE,
          &write_watcher_,
          this);
      return true;
    } else {
      message_send_bytes_written_ = 0;

      // Message sent OK!
      DVLOG(2) << "sent message @" << msg << " on channel @" << this
               << " with type " << msg->type() << " on fd " << pipe_;
      delete output_queue_.front();
      output_queue_.pop();
    }
  }
  return true;
}

bool Channel::ChannelImpl::Send(Message* message) {
  DVLOG(2) << "sending message @" << message << " on channel @" << this
           << " with type " << message->type()
           << " (" << output_queue_.size() << " in queue)";

#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging::GetInstance()->OnSendMessage(message, "");
#endif  // IPC_MESSAGE_LOG_ENABLED

  output_queue_.push(message);
  if (!is_blocked_on_write_ && !waiting_connect_) {
    return ProcessOutgoingMessages();
  }

  return true;
}

int Channel::ChannelImpl::GetClientFileDescriptor() const {
  return client_pipe_;
}

bool Channel::ChannelImpl::AcceptsConnections() const {
  return server_listen_pipe_ != -1;
}

bool Channel::ChannelImpl::HasAcceptedConnection() const {
  return AcceptsConnections() && pipe_ != -1;
}

void Channel::ChannelImpl::ResetToAcceptingConnectionState() {
  // Unregister libevent for the unix domain socket and close it.
  read_watcher_.StopWatchingFileDescriptor();
  write_watcher_.StopWatchingFileDescriptor();
  if (pipe_ != -1) {
    if (HANDLE_EINTR(close(pipe_)) < 0)
      PLOG(ERROR) << "close pipe_ " << pipe_name_;
    pipe_ = -1;
  }
#if defined(IPC_USES_READWRITE)
  if (fd_pipe_ != -1) {
    if (HANDLE_EINTR(close(fd_pipe_)) < 0)
      PLOG(ERROR) << "close fd_pipe_ " << pipe_name_;
    fd_pipe_ = -1;
  }
  if (remote_fd_pipe_ != -1) {
    if (HANDLE_EINTR(close(remote_fd_pipe_)) < 0)
      PLOG(ERROR) << "close remote_fd_pipe_ " << pipe_name_;
    remote_fd_pipe_ = -1;
  }
#endif  // IPC_USES_READWRITE

  while (!output_queue_.empty()) {
    Message* m = output_queue_.front();
    output_queue_.pop();
    delete m;
  }

  // Close any outstanding, received file descriptors.
  for (std::vector<int>::iterator
       i = input_overflow_fds_.begin(); i != input_overflow_fds_.end(); ++i) {
    if (HANDLE_EINTR(close(*i)) < 0)
      PLOG(ERROR) << "close";
  }
  input_overflow_fds_.clear();
}

// Called by libevent when we can read from the pipe without blocking.
void Channel::ChannelImpl::OnFileCanReadWithoutBlocking(int fd) {
  bool send_server_hello_msg = false;
  if (fd == server_listen_pipe_) {
    int new_pipe = 0;
    if (!ServerAcceptConnection(server_listen_pipe_, &new_pipe)) {
      Close();
      listener_->OnChannelListenError();
    }

    if (pipe_ != -1) {
      // We already have a connection. We only handle one at a time.
      // close our new descriptor.
      if (HANDLE_EINTR(shutdown(new_pipe, SHUT_RDWR)) < 0)
        PLOG(ERROR) << "shutdown " << pipe_name_;
      if (HANDLE_EINTR(close(new_pipe)) < 0)
        PLOG(ERROR) << "close " << pipe_name_;
      listener_->OnChannelDenied();
      return;
    }
    pipe_ = new_pipe;

    if (!AcceptConnection()) {
      NOTREACHED() << "AcceptConnection should not fail on server";
    }
    send_server_hello_msg = true;
    waiting_connect_ = false;
  } else if (fd == pipe_) {
    if (waiting_connect_ && (mode_ & MODE_SERVER_FLAG)) {
      send_server_hello_msg = true;
      waiting_connect_ = false;
    }
    if (!ProcessIncomingMessages()) {
      // ClosePipeOnError may delete this object, so we mustn't call
      // ProcessOutgoingMessages.
      send_server_hello_msg = false;
      ClosePipeOnError();
    }
  } else {
    NOTREACHED() << "Unknown pipe " << fd;
  }

  // If we're a server and handshaking, then we want to make sure that we
  // only send our handshake message after we've processed the client's.
  // This gives us a chance to kill the client if the incoming handshake
  // is invalid.
  if (send_server_hello_msg) {
    ProcessOutgoingMessages();
  }
}

// Called by libevent when we can write to the pipe without blocking.
void Channel::ChannelImpl::OnFileCanWriteWithoutBlocking(int fd) {
  DCHECK(fd == pipe_);
  is_blocked_on_write_ = false;
  if (!ProcessOutgoingMessages()) {
    ClosePipeOnError();
  }
}

bool Channel::ChannelImpl::AcceptConnection() {
  MessageLoopForIO::current()->WatchFileDescriptor(pipe_,
                                                   true,
                                                   MessageLoopForIO::WATCH_READ,
                                                   &read_watcher_,
                                                   this);
  QueueHelloMessage();

  if (mode_ & MODE_CLIENT_FLAG) {
    // If we are a client we want to send a hello message out immediately.
    // In server mode we will send a hello message when we receive one from a
    // client.
    waiting_connect_ = false;
    return ProcessOutgoingMessages();
  } else if (mode_ & MODE_SERVER_FLAG) {
    waiting_connect_ = true;
    return true;
  } else {
    NOTREACHED();
    return false;
  }
}

void Channel::ChannelImpl::ClosePipeOnError() {
  if (HasAcceptedConnection()) {
    ResetToAcceptingConnectionState();
    listener_->OnChannelError();
  } else {
    Close();
    if (AcceptsConnections()) {
      listener_->OnChannelListenError();
    } else {
      listener_->OnChannelError();
    }
  }
}

void Channel::ChannelImpl::QueueHelloMessage() {
  // Create the Hello message
  scoped_ptr<Message> msg(new Message(MSG_ROUTING_NONE,
                                      HELLO_MESSAGE_TYPE,
                                      IPC::Message::PRIORITY_NORMAL));

  if (!msg->WriteInt(base::GetCurrentProcId())) {
    NOTREACHED() << "Unable to pickle hello message proc id";
  }
#if defined(IPC_USES_READWRITE)
  scoped_ptr<Message> hello;
  if (remote_fd_pipe_ != -1) {
    if (!msg->WriteFileDescriptor(base::FileDescriptor(remote_fd_pipe_,
                                                       false))) {
      NOTREACHED() << "Unable to pickle hello message file descriptors";
    }
    DCHECK_EQ(msg->file_descriptor_set()->size(), 1U);
  }
#endif  // IPC_USES_READWRITE
  output_queue_.push(msg.release());
}

bool Channel::ChannelImpl::IsHelloMessage(const Message* m) const {
  return m->routing_id() == MSG_ROUTING_NONE && m->type() == HELLO_MESSAGE_TYPE;
}

void Channel::ChannelImpl::Close() {
  // Close can be called multiple time, so we need to make sure we're
  // idempotent.

  ResetToAcceptingConnectionState();

  if (must_unlink_) {
    unlink(pipe_name_.c_str());
    must_unlink_ = false;
  }
  if (server_listen_pipe_ != -1) {
    if (HANDLE_EINTR(close(server_listen_pipe_)) < 0)
      PLOG(ERROR) << "close " << server_listen_pipe_;
    server_listen_pipe_ = -1;
    // Unregister libevent for the listening socket and close it.
    server_listen_connection_watcher_.StopWatchingFileDescriptor();
  }

  if (client_pipe_ != -1) {
    PipeMap::GetInstance()->RemoveAndClose(pipe_name_);
    client_pipe_ = -1;
  }
}

//------------------------------------------------------------------------------
// Channel's methods simply call through to ChannelImpl.
Channel::Channel(const IPC::ChannelHandle& channel_handle, Mode mode,
                 Listener* listener)
    : channel_impl_(new ChannelImpl(channel_handle, mode, listener)) {
}

Channel::~Channel() {
  delete channel_impl_;
}

bool Channel::Connect() {
  return channel_impl_->Connect();
}

void Channel::Close() {
  channel_impl_->Close();
}

void Channel::set_listener(Listener* listener) {
  channel_impl_->set_listener(listener);
}

bool Channel::Send(Message* message) {
  return channel_impl_->Send(message);
}

int Channel::GetClientFileDescriptor() const {
  return channel_impl_->GetClientFileDescriptor();
}

bool Channel::AcceptsConnections() const {
  return channel_impl_->AcceptsConnections();
}

bool Channel::HasAcceptedConnection() const {
  return channel_impl_->HasAcceptedConnection();
}

void Channel::ResetToAcceptingConnectionState() {
  channel_impl_->ResetToAcceptingConnectionState();
}

}  // namespace IPC
