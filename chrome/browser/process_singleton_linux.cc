// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On Linux, when the user tries to launch a second copy of chrome, we check
// for a socket in the user's profile directory.  If the socket file is open we
// send a message to the first chrome browser process with the current
// directory and second process command line flags.  The second process then
// exits.
//
// We also have a lock file, which is a symlink to a non-existent destination.
// The destination is a string containing the hostname and process id of
// chrome's browser process, eg. "SingletonLock -> example.com-9156".  When the
// first copy of chrome exits it will delete the lock file on shutdown, so that
// a different instance on a different host may then use the profile directory.
//
// If writing to the socket fails, the hostname in the lock is checked to see if
// another instance is running a different host using a shared filesystem (nfs,
// etc.) If the hostname differs an error is displayed and the second process
// exits.  Otherwise the first process (if any) is killed and the second process
// starts as normal.
//
// When the second process sends the current directory and command line flags to
// the first process, it waits for an ACK message back from the first process
// for a certain time. If there is no ACK message back in time, then the first
// process will be considered as hung for some reason. The second process then
// retrieves the process id from the symbol link and kills it by sending
// SIGKILL. Then the second process starts as normal.
//
// TODO(james.su@gmail.com): Add unittest for this class.

#include "chrome/browser/process_singleton.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <set>
#include <string>

#include "app/l10n_util.h"
#include "base/base_paths.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/safe_strerror_posix.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#if defined(TOOLKIT_GTK)
#include "chrome/browser/gtk/process_singleton_dialog.h"
#endif
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"

const int ProcessSingleton::kTimeoutInSeconds;

namespace {

const char kStartToken[] = "START";
const char kACKToken[] = "ACK";
const char kShutdownToken[] = "SHUTDOWN";
const char kTokenDelimiter = '\0';
const int kMaxMessageLength = 32 * 1024;
const int kMaxACKMessageLength = arraysize(kShutdownToken) - 1;

const char kLockDelimiter = '-';

// Set a file descriptor to be non-blocking.
// Return 0 on success, -1 on failure.
int SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (-1 == flags)
    return flags;
  if (flags & O_NONBLOCK)
    return 0;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Set the close-on-exec bit on a file descriptor.
// Returns 0 on success, -1 on failure.
int SetCloseOnExec(int fd) {
  int flags = fcntl(fd, F_GETFD, 0);
  if (-1 == flags)
    return flags;
  if (flags & FD_CLOEXEC)
    return 0;
  return fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

// Close a socket and check return value.
void CloseSocket(int fd) {
  int rv = HANDLE_EINTR(close(fd));
  DCHECK_EQ(0, rv) << "Error closing socket: " << safe_strerror(errno);
}

// Write a message to a socket fd.
bool WriteToSocket(int fd, const char *message, size_t length) {
  DCHECK(message);
  DCHECK(length);
  size_t bytes_written = 0;
  do {
    ssize_t rv = HANDLE_EINTR(
        write(fd, message + bytes_written, length - bytes_written));
    if (rv < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // The socket shouldn't block, we're sending so little data.  Just give
        // up here, since NotifyOtherProcess() doesn't have an asynchronous api.
        LOG(ERROR) << "ProcessSingleton would block on write(), so it gave up.";
        return false;
      }
      PLOG(ERROR) << "write() failed";
      return false;
    }
    bytes_written += rv;
  } while (bytes_written < length);

  return true;
}

// Wait a socket for read for a certain timeout in seconds.
// Returns -1 if error occurred, 0 if timeout reached, > 0 if the socket is
// ready for read.
int WaitSocketForRead(int fd, int timeout) {
  fd_set read_fds;
  struct timeval tv;

  FD_ZERO(&read_fds);
  FD_SET(fd, &read_fds);
  tv.tv_sec = timeout;
  tv.tv_usec = 0;

  return HANDLE_EINTR(select(fd + 1, &read_fds, NULL, NULL, &tv));
}

// Read a message from a socket fd, with an optional timeout in seconds.
// If |timeout| <= 0 then read immediately.
// Return number of bytes actually read, or -1 on error.
ssize_t ReadFromSocket(int fd, char *buf, size_t bufsize, int timeout) {
  if (timeout > 0) {
    int rv = WaitSocketForRead(fd, timeout);
    if (rv <= 0)
      return rv;
  }

  size_t bytes_read = 0;
  do {
    ssize_t rv = HANDLE_EINTR(read(fd, buf + bytes_read, bufsize - bytes_read));
    if (rv < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        PLOG(ERROR) << "read() failed";
        return rv;
      } else {
        // It would block, so we just return what has been read.
        return bytes_read;
      }
    } else if (!rv) {
      // No more data to read.
      return bytes_read;
    } else {
      bytes_read += rv;
    }
  } while (bytes_read < bufsize);

  return bytes_read;
}

// Set up a socket and sockaddr appropriate for messaging.
void SetupSocket(const std::string& path, int* sock, struct sockaddr_un* addr) {
  *sock = socket(PF_UNIX, SOCK_STREAM, 0);
  PCHECK(*sock >= 0) << "socket() failed";

  int rv = SetNonBlocking(*sock);
  DCHECK_EQ(0, rv) << "Failed to make non-blocking socket.";
  rv = SetCloseOnExec(*sock);
  DCHECK_EQ(0, rv) << "Failed to set CLOEXEC on socket.";

  addr->sun_family = AF_UNIX;
  CHECK(path.length() < arraysize(addr->sun_path))
      << "Socket path too long: " << path;
  base::strlcpy(addr->sun_path, path.c_str(), arraysize(addr->sun_path));
}

// Read a symbol link, return empty string if given path is not a symbol link.
std::string ReadLink(const std::string& path) {
  struct stat statbuf;

  if (lstat(path.c_str(), &statbuf) < 0) {
    DCHECK_EQ(errno, ENOENT);
    return std::string();
  }

  if (S_ISLNK(statbuf.st_mode)) {
    char buf[PATH_MAX + 1];
    ssize_t len = readlink(path.c_str(), buf, PATH_MAX);
    if (len > 0) {
      buf[len] = '\0';
      return std::string(buf);
    } else {
      PLOG(ERROR) << "readlink(" << path << ") failed";
    }
  }

  return std::string();
}

// Unlink a path. Return true on success.
bool UnlinkPath(const std::string& path) {
  int rv = unlink(path.c_str());
  if (rv < 0 && errno != ENOENT)
    PLOG(ERROR) << "Failed to unlink " << path;

  return rv == 0;
}

// Extract the hostname and pid from the lock symlink.
std::string ParseLockPath(const std::string& path,
                          std::string* hostname,
                          int* pid) {
  std::string real_path = ReadLink(path);
  std::string::size_type pos = real_path.rfind('-');

  // If the path is not a symbolic link, or doesn't contain what we expect,
  // bail.
  if (pos == std::string::npos) {
    *hostname = "";
    *pid = -1;
    return "";
  }

  *hostname = real_path.substr(0, pos);

  const std::string& pid_str = real_path.substr(pos + 1);
  if (!StringToInt(pid_str, pid))
    *pid = -1;

  return real_path;
}

void DisplayProfileInUseError(const std::string& lock_path,
                              const std::string& hostname,
                              int pid) {
  std::wstring error = l10n_util::GetStringF(IDS_PROFILE_IN_USE_LINUX,
        IntToWString(pid),
        ASCIIToWide(hostname),
        base::SysNativeMBToWide(lock_path),
        l10n_util::GetString(IDS_PRODUCT_NAME));
  LOG(ERROR) << base::SysWideToNativeMB(error).c_str();
#if defined(TOOLKIT_GTK)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kNoProcessSingletonDialog))
    ProcessSingletonDialog::ShowAndRun(WideToUTF8(error));
#endif
}

// Check if the lock is on a different host.  If so, return false.  If not,
// unlink the lock file and return true.
bool CheckLockHostnameAndCleanup(const std::string& path) {
  std::string hostname;
  int pid;
  ParseLockPath(path, &hostname, &pid);

  if (!hostname.empty() && hostname != net::GetHostName()) {
    DisplayProfileInUseError(path, hostname, pid);
    return false;
  }
  UnlinkPath(path);
  return true;
}

// Extract the process's pid from a symbol link path and if it is on
// the same host, kill the process, unlink the lock file and return true.
// If the process is on a different host, return false.
bool KillProcessByLockPath(const std::string& path) {
  std::string hostname;
  int pid;
  ParseLockPath(path, &hostname, &pid);

  if (!hostname.empty() && hostname != net::GetHostName()) {
    DisplayProfileInUseError(path, hostname, pid);
    return false;
  }
  UnlinkPath(path);

  if (pid >= 0) {
    // TODO(james.su@gmail.com): Is SIGKILL ok?
    int rv = kill(static_cast<base::ProcessHandle>(pid), SIGKILL);
    DCHECK_EQ(0, rv) << "Error killing process: " << safe_strerror(errno);
    return true;
  }

  LOG(ERROR) << "Failed to extract pid from path: " << path;
  return true;
}

// A helper class to close a socket automatically.
class SocketCloser {
 public:
  explicit SocketCloser(int fd) : fd_(fd) { }
  ~SocketCloser() { CloseSocket(fd_); }
 private:
  int fd_;
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ProcessSingleton::LinuxWatcher
// A helper class for a Linux specific implementation of the process singleton.
// This class sets up a listener on the singleton socket and handles parsing
// messages that come in on the singleton socket.
class ProcessSingleton::LinuxWatcher
    : public MessageLoopForIO::Watcher,
      public MessageLoop::DestructionObserver,
      public base::RefCountedThreadSafe<ProcessSingleton::LinuxWatcher> {
 public:
  // A helper class to read message from an established socket.
  class SocketReader : public MessageLoopForIO::Watcher {
   public:
    SocketReader(ProcessSingleton::LinuxWatcher* parent,
                 MessageLoop* ui_message_loop,
                 int fd)
        : parent_(parent),
          ui_message_loop_(ui_message_loop),
          fd_(fd),
          bytes_read_(0) {
      // Wait for reads.
      MessageLoopForIO::current()->WatchFileDescriptor(
          fd, true, MessageLoopForIO::WATCH_READ, &fd_reader_, this);
      timer_.Start(base::TimeDelta::FromSeconds(kTimeoutInSeconds),
                   this, &SocketReader::OnTimerExpiry);
    }

    virtual ~SocketReader() {
      CloseSocket(fd_);
    }

    // MessageLoopForIO::Watcher impl.
    virtual void OnFileCanReadWithoutBlocking(int fd);
    virtual void OnFileCanWriteWithoutBlocking(int fd) {
      // SocketReader only watches for accept (read) events.
      NOTREACHED();
    }

    // Finish handling the incoming message by optionally sending back an ACK
    // message and removing this SocketReader.
    void FinishWithACK(const char *message, size_t length);

   private:
    // If we haven't completed in a reasonable amount of time, give up.
    void OnTimerExpiry() {
      parent_->RemoveSocketReader(this);
      // We're deleted beyond this point.
    }

    MessageLoopForIO::FileDescriptorWatcher fd_reader_;

    // The ProcessSingleton::LinuxWatcher that owns us.
    ProcessSingleton::LinuxWatcher* const parent_;

    // A reference to the UI message loop.
    MessageLoop* const ui_message_loop_;

    // The file descriptor we're reading.
    const int fd_;

    // Store the message in this buffer.
    char buf_[kMaxMessageLength];

    // Tracks the number of bytes we've read in case we're getting partial
    // reads.
    size_t bytes_read_;

    base::OneShotTimer<SocketReader> timer_;

    DISALLOW_COPY_AND_ASSIGN(SocketReader);
  };

  // We expect to only be constructed on the UI thread.
  explicit LinuxWatcher(ProcessSingleton* parent)
      : ui_message_loop_(MessageLoop::current()),
        parent_(parent) {
  }

  virtual ~LinuxWatcher() {
    STLDeleteElements(&readers_);
  }

  // Start listening for connections on the socket.  This method should be
  // called from the IO thread.
  void StartListening(int socket);

  // This method determines if we should use the same process and if we should,
  // opens a new browser tab.  This runs on the UI thread.
  // |reader| is for sending back ACK message.
  void HandleMessage(const std::string& current_dir,
                     const std::vector<std::string>& argv,
                     SocketReader *reader);

  // MessageLoopForIO::Watcher impl.  These run on the IO thread.
  virtual void OnFileCanReadWithoutBlocking(int fd);
  virtual void OnFileCanWriteWithoutBlocking(int fd) {
    // ProcessSingleton only watches for accept (read) events.
    NOTREACHED();
  }

  // MessageLoop::DestructionObserver
  virtual void WillDestroyCurrentMessageLoop() {
    fd_watcher_.StopWatchingFileDescriptor();
  }

 private:
  // Removes and deletes the SocketReader.
  void RemoveSocketReader(SocketReader* reader);

  MessageLoopForIO::FileDescriptorWatcher fd_watcher_;

  // A reference to the UI message loop (i.e., the message loop we were
  // constructed on).
  MessageLoop* ui_message_loop_;

  // The ProcessSingleton that owns us.
  ProcessSingleton* const parent_;

  std::set<SocketReader*> readers_;

  DISALLOW_COPY_AND_ASSIGN(LinuxWatcher);
};

void ProcessSingleton::LinuxWatcher::OnFileCanReadWithoutBlocking(int fd) {
  // Accepting incoming client.
  sockaddr_un from;
  socklen_t from_len = sizeof(from);
  int connection_socket = HANDLE_EINTR(accept(
      fd, reinterpret_cast<sockaddr*>(&from), &from_len));
  if (-1 == connection_socket) {
    PLOG(ERROR) << "accept() failed";
    return;
  }
  int rv = SetNonBlocking(connection_socket);
  DCHECK_EQ(0, rv) << "Failed to make non-blocking socket.";
  SocketReader* reader = new SocketReader(this,
                                          ui_message_loop_,
                                          connection_socket);
  readers_.insert(reader);
}

void ProcessSingleton::LinuxWatcher::StartListening(int socket) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  // Watch for client connections on this socket.
  MessageLoopForIO* ml = MessageLoopForIO::current();
  ml->AddDestructionObserver(this);
  ml->WatchFileDescriptor(socket, true, MessageLoopForIO::WATCH_READ,
                          &fd_watcher_, this);
}

void ProcessSingleton::LinuxWatcher::HandleMessage(
    const std::string& current_dir, const std::vector<std::string>& argv,
    SocketReader* reader) {
  DCHECK(ui_message_loop_ == MessageLoop::current());
  DCHECK(reader);
  // If locked, it means we are not ready to process this message because
  // we are probably in a first run critical phase.
  if (parent_->locked()) {
    DLOG(WARNING) << "Browser is locked";
    // Send back "ACK" message to prevent the client process from starting up.
    reader->FinishWithACK(kACKToken, arraysize(kACKToken) - 1);
    return;
  }

  // Ignore the request if the browser process is already in shutdown path.
  if (!g_browser_process || g_browser_process->IsShuttingDown()) {
    LOG(WARNING) << "Not handling interprocess notification as browser"
                    " is shutting down";
    // Send back "SHUTDOWN" message, so that the client process can start up
    // without killing this process.
    reader->FinishWithACK(kShutdownToken, arraysize(kShutdownToken) - 1);
    return;
  }

  CommandLine parsed_command_line(argv);
  PrefService* prefs = g_browser_process->local_state();
  DCHECK(prefs);

  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
  if (!profile) {
    // We should only be able to get here if the profile already exists and
    // has been created.
    NOTREACHED();
    return;
  }

  // Run the browser startup sequence again, with the command line of the
  // signalling process.
  FilePath current_dir_file_path(current_dir);
  BrowserInit::ProcessCommandLine(parsed_command_line,
                                  current_dir_file_path.ToWStringHack(),
                                  false, profile, NULL);

  // Send back "ACK" message to prevent the client process from starting up.
  reader->FinishWithACK(kACKToken, arraysize(kACKToken) - 1);
}

void ProcessSingleton::LinuxWatcher::RemoveSocketReader(SocketReader* reader) {
  DCHECK(reader);
  readers_.erase(reader);
  delete reader;
}

///////////////////////////////////////////////////////////////////////////////
// ProcessSingleton::LinuxWatcher::SocketReader
//

void ProcessSingleton::LinuxWatcher::SocketReader::OnFileCanReadWithoutBlocking(
    int fd) {
  DCHECK_EQ(fd, fd_);
  while (bytes_read_ < sizeof(buf_)) {
    ssize_t rv = HANDLE_EINTR(
        read(fd, buf_ + bytes_read_, sizeof(buf_) - bytes_read_));
    if (rv < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        PLOG(ERROR) << "read() failed";
        CloseSocket(fd);
        return;
      } else {
        // It would block, so we just return and continue to watch for the next
        // opportunity to read.
        return;
      }
    } else if (!rv) {
      // No more data to read.  It's time to process the message.
      break;
    } else {
      bytes_read_ += rv;
    }
  }

  // Validate the message.  The shortest message is kStartToken\0x\0x
  const size_t kMinMessageLength = arraysize(kStartToken) + 4;
  if (bytes_read_ < kMinMessageLength) {
    buf_[bytes_read_] = 0;
    LOG(ERROR) << "Invalid socket message (wrong length):" << buf_;
    return;
  }

  std::string str(buf_, bytes_read_);
  std::vector<std::string> tokens;
  SplitString(str, kTokenDelimiter, &tokens);

  if (tokens.size() < 3 || tokens[0] != kStartToken) {
    LOG(ERROR) << "Wrong message format: " << str;
    return;
  }

  // Stop the expiration timer to prevent this SocketReader object from being
  // terminated unexpectly.
  timer_.Stop();

  std::string current_dir = tokens[1];
  // Remove the first two tokens.  The remaining tokens should be the command
  // line argv array.
  tokens.erase(tokens.begin());
  tokens.erase(tokens.begin());

  // Return to the UI thread to handle opening a new browser tab.
  ui_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      parent_,
      &ProcessSingleton::LinuxWatcher::HandleMessage,
      current_dir,
      tokens,
      this));
  fd_reader_.StopWatchingFileDescriptor();

  // LinuxWatcher::HandleMessage() is in charge of destroying this SocketReader
  // object by invoking SocketReader::FinishWithACK().
}

void ProcessSingleton::LinuxWatcher::SocketReader::FinishWithACK(
    const char *message, size_t length) {
  if (message && length) {
    // Not necessary to care about the return value.
    WriteToSocket(fd_, message, length);
  }

  if (shutdown(fd_, SHUT_WR) < 0)
    PLOG(ERROR) << "shutdown() failed";

  parent_->RemoveSocketReader(this);
  // We are deleted beyond this point.
}

///////////////////////////////////////////////////////////////////////////////
// ProcessSingleton
//
ProcessSingleton::ProcessSingleton(const FilePath& user_data_dir)
    : locked_(false),
      foreground_window_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(watcher_(new LinuxWatcher(this))) {
  socket_path_ = user_data_dir.Append(chrome::kSingletonSocketFilename);
  lock_path_ = user_data_dir.Append(chrome::kSingletonLockFilename);
}

ProcessSingleton::~ProcessSingleton() {
}

ProcessSingleton::NotifyResult ProcessSingleton::NotifyOtherProcess() {
  return NotifyOtherProcessWithTimeout(*CommandLine::ForCurrentProcess(),
                                       kTimeoutInSeconds);
}

ProcessSingleton::NotifyResult ProcessSingleton::NotifyOtherProcessWithTimeout(
    const CommandLine& cmd_line,
    int timeout_seconds) {
  int socket;
  sockaddr_un addr;
  SetupSocket(socket_path_.value(), &socket, &addr);

  // It'll close the socket automatically when exiting this method.
  SocketCloser socket_closer(socket);

  // Connecting to the socket
  int ret = HANDLE_EINTR(connect(socket,
                                 reinterpret_cast<sockaddr*>(&addr),
                                 sizeof(addr)));
  if (ret < 0) {
    // TODO(mattm): there is a bit of a race here if another instance on the
    // same host is in Create() and has created the lock but not attached to the
    // socket.  Our CheckLockHostnameAndCleanup call will clean up their lock
    // and allow us to create a new one.
    if (!CheckLockHostnameAndCleanup(lock_path_.value()))
      return PROFILE_IN_USE;
    return PROCESS_NONE;  // Tell the caller there's nobody to notify.
  }

  timeval timeout = {timeout_seconds, 0};
  setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  // Found another process, prepare our command line
  // format is "START\0<current dir>\0<argv[0]>\0...\0<argv[n]>".
  std::string to_send(kStartToken);
  to_send.push_back(kTokenDelimiter);

  FilePath current_dir;
  if (!PathService::Get(base::DIR_CURRENT, &current_dir))
    return PROCESS_NONE;
  to_send.append(current_dir.value());

  const std::vector<std::string>& argv = cmd_line.argv();
  for (std::vector<std::string>::const_iterator it = argv.begin();
      it != argv.end(); ++it) {
    to_send.push_back(kTokenDelimiter);
    to_send.append(*it);
  }

  // Send the message
  if (!WriteToSocket(socket, to_send.data(), to_send.length())) {
    // Try to kill the other process, because it might have been dead.
    if (!KillProcessByLockPath(lock_path_.value()))
      return PROFILE_IN_USE;
    return PROCESS_NONE;
  }

  if (shutdown(socket, SHUT_WR) < 0)
    PLOG(ERROR) << "shutdown() failed";

  // Read ACK message from the other process. It might be blocked for a certain
  // timeout, to make sure the other process has enough time to return ACK.
  char buf[kMaxACKMessageLength + 1];
  ssize_t len =
      ReadFromSocket(socket, buf, kMaxACKMessageLength, timeout_seconds);

  // Failed to read ACK, the other process might have been frozen.
  if (len <= 0) {
    if (!KillProcessByLockPath(lock_path_.value()))
      return PROFILE_IN_USE;
    return PROCESS_NONE;
  }

  buf[len] = '\0';
  if (strncmp(buf, kShutdownToken, arraysize(kShutdownToken) - 1) == 0) {
    // The other process is shutting down, it's safe to start a new process.
    return PROCESS_NONE;
  } else if (strncmp(buf, kACKToken, arraysize(kACKToken) - 1) == 0) {
    // Assume the other process is handling the request.
    return PROCESS_NOTIFIED;
  }

  NOTREACHED() << "The other process returned unknown message: " << buf;
  return PROCESS_NOTIFIED;
}

void ProcessSingleton::Create() {
  int sock;
  sockaddr_un addr;

  // The symlink lock is pointed to the hostname and process id, so other
  // processes can find it out.
  std::string symlink_content = StringPrintf(
      "%s%c%u",
      net::GetHostName().c_str(),
      kLockDelimiter,
      base::GetCurrentProcId());

  // Create symbol link before binding the socket, to ensure only one instance
  // can have the socket open.
  if (symlink(symlink_content.c_str(), lock_path_.value().c_str()) < 0) {
    // Double check the value in case symlink suceeded but we got an incorrect
    // failure due to NFS packet loss & retry.
    int saved_errno = errno;
    if (ReadLink(lock_path_.value()) != symlink_content) {
      // If we failed to create the lock, most likely another instance won the
      // startup race.
      // TODO(mattm): If the other instance is on the same host, we could try
      // to notify it rather than just failing.
      errno = saved_errno;
      PLOG(FATAL) << "Failed to create " << lock_path_.value();
    }
  }

  SetupSocket(socket_path_.value(), &sock, &addr);

  UnlinkPath(socket_path_.value());

  if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    PLOG(ERROR) << "bind() failed";
    LOG(ERROR) << "SingletonSocket failed to create a socket in your home "
                  "directory. This means that running multiple instances of "
                  "the Chrome binary will start multiple browser process "
                  "rather than opening a new window in the existing process.";
    CloseSocket(sock);
    return;
  }

  if (listen(sock, 5) < 0)
    NOTREACHED() << "listen failed: " << safe_strerror(errno);

  // Normally we would use ChromeThread, but the IO thread hasn't started yet.
  // Using g_browser_process, we start the thread so we can listen on the
  // socket.
  MessageLoop* ml = g_browser_process->io_thread()->message_loop();
  DCHECK(ml);
  ml->PostTask(FROM_HERE, NewRunnableMethod(
    watcher_.get(),
    &ProcessSingleton::LinuxWatcher::StartListening,
    sock));
}

void ProcessSingleton::Cleanup() {
  UnlinkPath(lock_path_.value());
}
