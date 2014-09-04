// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/unix_domain_listen_socket_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <queue>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/posix/eintr_wrapper.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "net/socket/socket_descriptor.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::queue;
using std::string;

namespace net {
namespace deprecated {
namespace {

const char kSocketFilename[] = "socket_for_testing";
const char kInvalidSocketPath[] = "/invalid/path";
const char kMsg[] = "hello";

enum EventType {
  EVENT_ACCEPT,
  EVENT_AUTH_DENIED,
  EVENT_AUTH_GRANTED,
  EVENT_CLOSE,
  EVENT_LISTEN,
  EVENT_READ,
};

class EventManager : public base::RefCounted<EventManager> {
 public:
  EventManager() : condition_(&mutex_) {}

  bool HasPendingEvent() {
    base::AutoLock lock(mutex_);
    return !events_.empty();
  }

  void Notify(EventType event) {
    base::AutoLock lock(mutex_);
    events_.push(event);
    condition_.Broadcast();
  }

  EventType WaitForEvent() {
    base::AutoLock lock(mutex_);
    while (events_.empty())
      condition_.Wait();
    EventType event = events_.front();
    events_.pop();
    return event;
  }

 private:
  friend class base::RefCounted<EventManager>;
  virtual ~EventManager() {}

  queue<EventType> events_;
  base::Lock mutex_;
  base::ConditionVariable condition_;
};

class TestListenSocketDelegate : public StreamListenSocket::Delegate {
 public:
  explicit TestListenSocketDelegate(
      const scoped_refptr<EventManager>& event_manager)
      : event_manager_(event_manager) {}

  virtual void DidAccept(StreamListenSocket* server,
                         scoped_ptr<StreamListenSocket> connection) OVERRIDE {
    LOG(ERROR) << __PRETTY_FUNCTION__;
    connection_ = connection.Pass();
    Notify(EVENT_ACCEPT);
  }

  virtual void DidRead(StreamListenSocket* connection,
                       const char* data,
                       int len) OVERRIDE {
    {
      base::AutoLock lock(mutex_);
      DCHECK(len);
      data_.assign(data, len - 1);
    }
    Notify(EVENT_READ);
  }

  virtual void DidClose(StreamListenSocket* sock) OVERRIDE {
    Notify(EVENT_CLOSE);
  }

  void OnListenCompleted() {
    Notify(EVENT_LISTEN);
  }

  string ReceivedData() {
    base::AutoLock lock(mutex_);
    return data_;
  }

 private:
  void Notify(EventType event) {
    event_manager_->Notify(event);
  }

  const scoped_refptr<EventManager> event_manager_;
  scoped_ptr<StreamListenSocket> connection_;
  base::Lock mutex_;
  string data_;
};

bool UserCanConnectCallback(
    bool allow_user, const scoped_refptr<EventManager>& event_manager,
    const UnixDomainServerSocket::Credentials&) {
  event_manager->Notify(
      allow_user ? EVENT_AUTH_GRANTED : EVENT_AUTH_DENIED);
  return allow_user;
}

class UnixDomainListenSocketTestHelper : public testing::Test {
 public:
  void CreateAndListen() {
    socket_ = UnixDomainListenSocket::CreateAndListen(
        file_path_.value(), socket_delegate_.get(), MakeAuthCallback());
    socket_delegate_->OnListenCompleted();
  }

 protected:
  UnixDomainListenSocketTestHelper(const string& path_str, bool allow_user)
      : allow_user_(allow_user) {
    file_path_ = base::FilePath(path_str);
    if (!file_path_.IsAbsolute()) {
      EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
      file_path_ = GetTempSocketPath(file_path_.value());
    }
    // Beware that if path_str is an absolute path, this class doesn't delete
    // the file. It must be an invalid path and cannot be created by unittests.
  }

  base::FilePath GetTempSocketPath(const std::string socket_name) {
    DCHECK(temp_dir_.IsValid());
    return temp_dir_.path().Append(socket_name);
  }

  virtual void SetUp() OVERRIDE {
    event_manager_ = new EventManager();
    socket_delegate_.reset(new TestListenSocketDelegate(event_manager_));
  }

  virtual void TearDown() OVERRIDE {
    socket_.reset();
    socket_delegate_.reset();
    event_manager_ = NULL;
  }

  UnixDomainListenSocket::AuthCallback MakeAuthCallback() {
    return base::Bind(&UserCanConnectCallback, allow_user_, event_manager_);
  }

  SocketDescriptor CreateClientSocket() {
    const SocketDescriptor sock = CreatePlatformSocket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
      LOG(ERROR) << "socket() error";
      return kInvalidSocket;
    }
    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    socklen_t addr_len;
    strncpy(addr.sun_path, file_path_.value().c_str(), sizeof(addr.sun_path));
    addr_len = sizeof(sockaddr_un);
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), addr_len) != 0) {
      LOG(ERROR) << "connect() error: " << strerror(errno)
                 << ": path=" << file_path_.value();
      return kInvalidSocket;
    }
    return sock;
  }

  scoped_ptr<base::Thread> CreateAndRunServerThread() {
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    scoped_ptr<base::Thread> thread(new base::Thread("socketio_test"));
    thread->StartWithOptions(options);
    thread->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&UnixDomainListenSocketTestHelper::CreateAndListen,
                   base::Unretained(this)));
    return thread.Pass();
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath file_path_;
  const bool allow_user_;
  scoped_refptr<EventManager> event_manager_;
  scoped_ptr<TestListenSocketDelegate> socket_delegate_;
  scoped_ptr<UnixDomainListenSocket> socket_;
};

class UnixDomainListenSocketTest : public UnixDomainListenSocketTestHelper {
 protected:
  UnixDomainListenSocketTest()
      : UnixDomainListenSocketTestHelper(kSocketFilename,
                                         true /* allow user */) {}
};

class UnixDomainListenSocketTestWithInvalidPath
    : public UnixDomainListenSocketTestHelper {
 protected:
  UnixDomainListenSocketTestWithInvalidPath()
      : UnixDomainListenSocketTestHelper(kInvalidSocketPath, true) {}
};

class UnixDomainListenSocketTestWithForbiddenUser
    : public UnixDomainListenSocketTestHelper {
 protected:
  UnixDomainListenSocketTestWithForbiddenUser()
      : UnixDomainListenSocketTestHelper(kSocketFilename,
                                         false /* forbid user */) {}
};

TEST_F(UnixDomainListenSocketTest, CreateAndListen) {
  CreateAndListen();
  EXPECT_FALSE(socket_.get() == NULL);
}

TEST_F(UnixDomainListenSocketTestWithInvalidPath,
       CreateAndListenWithInvalidPath) {
  CreateAndListen();
  EXPECT_TRUE(socket_.get() == NULL);
}

#ifdef SOCKET_ABSTRACT_NAMESPACE_SUPPORTED
// Test with an invalid path to make sure that the socket is not backed by a
// file.
TEST_F(UnixDomainListenSocketTestWithInvalidPath,
       CreateAndListenWithAbstractNamespace) {
  socket_ = UnixDomainListenSocket::CreateAndListenWithAbstractNamespace(
      file_path_.value(), "", socket_delegate_.get(), MakeAuthCallback());
  EXPECT_FALSE(socket_.get() == NULL);
}

TEST_F(UnixDomainListenSocketTest, TestFallbackName) {
  scoped_ptr<UnixDomainListenSocket> existing_socket =
      UnixDomainListenSocket::CreateAndListenWithAbstractNamespace(
          file_path_.value(), "", socket_delegate_.get(), MakeAuthCallback());
  EXPECT_FALSE(existing_socket.get() == NULL);
  // First, try to bind socket with the same name with no fallback name.
  socket_ =
      UnixDomainListenSocket::CreateAndListenWithAbstractNamespace(
          file_path_.value(), "", socket_delegate_.get(), MakeAuthCallback());
  EXPECT_TRUE(socket_.get() == NULL);
  // Now with a fallback name.
  const char kFallbackSocketName[] = "socket_for_testing_2";
  socket_ = UnixDomainListenSocket::CreateAndListenWithAbstractNamespace(
      file_path_.value(),
      GetTempSocketPath(kFallbackSocketName).value(),
      socket_delegate_.get(),
      MakeAuthCallback());
  EXPECT_FALSE(socket_.get() == NULL);
}
#endif

TEST_F(UnixDomainListenSocketTest, TestWithClient) {
  const scoped_ptr<base::Thread> server_thread = CreateAndRunServerThread();
  EventType event = event_manager_->WaitForEvent();
  ASSERT_EQ(EVENT_LISTEN, event);

  // Create the client socket.
  const SocketDescriptor sock = CreateClientSocket();
  ASSERT_NE(kInvalidSocket, sock);
  event = event_manager_->WaitForEvent();
  ASSERT_EQ(EVENT_AUTH_GRANTED, event);
  event = event_manager_->WaitForEvent();
  ASSERT_EQ(EVENT_ACCEPT, event);

  // Send a message from the client to the server.
  ssize_t ret = HANDLE_EINTR(send(sock, kMsg, sizeof(kMsg), 0));
  ASSERT_NE(-1, ret);
  ASSERT_EQ(sizeof(kMsg), static_cast<size_t>(ret));
  event = event_manager_->WaitForEvent();
  ASSERT_EQ(EVENT_READ, event);
  ASSERT_EQ(kMsg, socket_delegate_->ReceivedData());

  // Close the client socket.
  ret = IGNORE_EINTR(close(sock));
  event = event_manager_->WaitForEvent();
  ASSERT_EQ(EVENT_CLOSE, event);
}

TEST_F(UnixDomainListenSocketTestWithForbiddenUser, TestWithForbiddenUser) {
  const scoped_ptr<base::Thread> server_thread = CreateAndRunServerThread();
  EventType event = event_manager_->WaitForEvent();
  ASSERT_EQ(EVENT_LISTEN, event);
  const SocketDescriptor sock = CreateClientSocket();
  ASSERT_NE(kInvalidSocket, sock);

  event = event_manager_->WaitForEvent();
  ASSERT_EQ(EVENT_AUTH_DENIED, event);

  // Wait until the file descriptor is closed by the server.
  struct pollfd poll_fd;
  poll_fd.fd = sock;
  poll_fd.events = POLLIN;
  poll(&poll_fd, 1, -1 /* rely on GTest for timeout handling */);

  // Send() must fail.
  ssize_t ret = HANDLE_EINTR(send(sock, kMsg, sizeof(kMsg), 0));
  ASSERT_EQ(-1, ret);
  ASSERT_EQ(EPIPE, errno);
  ASSERT_FALSE(event_manager_->HasPendingEvent());
}

}  // namespace
}  // namespace deprecated
}  // namespace net
