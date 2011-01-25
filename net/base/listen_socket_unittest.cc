// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/listen_socket_unittest.h"

#include <fcntl.h>
#include <sys/types.h>

#include "base/eintr_wrapper.h"
#include "net/base/net_util.h"
#include "testing/platform_test.h"

const int ListenSocketTester::kTestPort = 9999;

static const int kReadBufSize = 1024;
static const char kHelloWorld[] = "HELLO, WORLD";
static const int kMaxQueueSize = 20;
static const char kLoopback[] = "127.0.0.1";
static const int kDefaultTimeoutMs = 5000;

ListenSocket* ListenSocketTester::DoListen() {
  return ListenSocket::Listen(kLoopback, kTestPort, this);
}

void ListenSocketTester::SetUp() {
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  thread_.reset(new base::Thread("socketio_test"));
  thread_->StartWithOptions(options);
  loop_ = reinterpret_cast<MessageLoopForIO*>(thread_->message_loop());

  loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &ListenSocketTester::Listen));

  // verify Listen succeeded
  NextAction();
  ASSERT_FALSE(server_ == NULL);
  ASSERT_EQ(ACTION_LISTEN, last_action_.type());

  // verify the connect/accept and setup test_socket_
  test_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_NE(INVALID_SOCKET, test_socket_);
  struct sockaddr_in client;
  client.sin_family = AF_INET;
  client.sin_addr.s_addr = inet_addr(kLoopback);
  client.sin_port = htons(kTestPort);
  int ret = HANDLE_EINTR(
      connect(test_socket_, reinterpret_cast<sockaddr*>(&client),
              sizeof(client)));
  ASSERT_NE(ret, SOCKET_ERROR);

  NextAction();
  ASSERT_EQ(ACTION_ACCEPT, last_action_.type());
}

void ListenSocketTester::TearDown() {
#if defined(OS_WIN)
  ASSERT_EQ(0, closesocket(test_socket_));
#elif defined(OS_POSIX)
  ASSERT_EQ(0, HANDLE_EINTR(close(test_socket_)));
#endif
  NextAction();
  ASSERT_EQ(ACTION_CLOSE, last_action_.type());

  loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &ListenSocketTester::Shutdown));
  NextAction();
  ASSERT_EQ(ACTION_SHUTDOWN, last_action_.type());

  thread_.reset();
  loop_ = NULL;
}

void ListenSocketTester::ReportAction(const ListenSocketTestAction& action) {
  base::AutoLock locked(lock_);
  queue_.push_back(action);
  cv_.Broadcast();
}

void ListenSocketTester::NextAction() {
  base::AutoLock locked(lock_);
  while (queue_.empty())
    cv_.Wait();
  last_action_ = queue_.front();
  queue_.pop_front();
}

int ListenSocketTester::ClearTestSocket() {
  char buf[kReadBufSize];
  int len_ret = 0;
  do {
    int len = HANDLE_EINTR(recv(test_socket_, buf, kReadBufSize, 0));
    if (len == SOCKET_ERROR || len == 0) {
      break;
    } else {
      len_ret += len;
    }
  } while (true);
  return len_ret;
}

void ListenSocketTester::Shutdown() {
  connection_->Release();
  connection_ = NULL;
  server_->Release();
  server_ = NULL;
  ReportAction(ListenSocketTestAction(ACTION_SHUTDOWN));
}

void ListenSocketTester::Listen() {
  server_ = DoListen();
  if (server_) {
    server_->AddRef();
    ReportAction(ListenSocketTestAction(ACTION_LISTEN));
  }
}

void ListenSocketTester::SendFromTester() {
  connection_->Send(kHelloWorld);
  ReportAction(ListenSocketTestAction(ACTION_SEND));
}

void ListenSocketTester::DidAccept(ListenSocket *server,
                                   ListenSocket *connection) {
  connection_ = connection;
  connection_->AddRef();
  ReportAction(ListenSocketTestAction(ACTION_ACCEPT));
}

void ListenSocketTester::DidRead(ListenSocket *connection,
                                 const char* data,
                                 int len) {
  std::string str(data, len);
  ReportAction(ListenSocketTestAction(ACTION_READ, str));
}

void ListenSocketTester::DidClose(ListenSocket *sock) {
  ReportAction(ListenSocketTestAction(ACTION_CLOSE));
}

bool ListenSocketTester::Send(SOCKET sock, const std::string& str) {
  int len = static_cast<int>(str.length());
  int send_len = HANDLE_EINTR(send(sock, str.data(), len, 0));
  if (send_len == SOCKET_ERROR) {
    LOG(ERROR) << "send failed: " << errno;
    return false;
  } else if (send_len != len) {
    return false;
  }
  return true;
}

void ListenSocketTester::TestClientSend() {
  ASSERT_TRUE(Send(test_socket_, kHelloWorld));
  NextAction();
  ASSERT_EQ(ACTION_READ, last_action_.type());
  ASSERT_EQ(last_action_.data(), kHelloWorld);
}

void ListenSocketTester::TestClientSendLong() {
  size_t hello_len = strlen(kHelloWorld);
  std::string long_string;
  size_t long_len = 0;
  for (int i = 0; i < 200; i++) {
    long_string += kHelloWorld;
    long_len += hello_len;
  }
  ASSERT_TRUE(Send(test_socket_, long_string));
  size_t read_len = 0;
  while (read_len < long_len) {
    NextAction();
    ASSERT_EQ(ACTION_READ, last_action_.type());
    std::string last_data = last_action_.data();
    size_t len = last_data.length();
    if (long_string.compare(read_len, len, last_data)) {
      ASSERT_EQ(long_string.compare(read_len, len, last_data), 0);
    }
    read_len += last_data.length();
  }
  ASSERT_EQ(read_len, long_len);
}

void ListenSocketTester::TestServerSend() {
  loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &ListenSocketTester::SendFromTester));
  NextAction();
  ASSERT_EQ(ACTION_SEND, last_action_.type());
  const int buf_len = 200;
  char buf[buf_len+1];
  unsigned recv_len = 0;
  while (recv_len < strlen(kHelloWorld)) {
    int r = HANDLE_EINTR(recv(test_socket_, buf, buf_len, 0));
    ASSERT_GE(r, 0);
    recv_len += static_cast<unsigned>(r);
    if (!r)
      break;
  }
  buf[recv_len] = 0;
  ASSERT_STREQ(buf, kHelloWorld);
}


class ListenSocketTest: public PlatformTest {
 public:
  ListenSocketTest() {
    tester_ = NULL;
  }

  virtual void SetUp() {
    PlatformTest::SetUp();
    tester_ = new ListenSocketTester();
    tester_->SetUp();
  }

  virtual void TearDown() {
    PlatformTest::TearDown();
    tester_->TearDown();
    tester_ = NULL;
  }

  scoped_refptr<ListenSocketTester> tester_;
};

TEST_F(ListenSocketTest, ClientSend) {
  tester_->TestClientSend();
}

TEST_F(ListenSocketTest, ClientSendLong) {
  tester_->TestClientSendLong();
}

TEST_F(ListenSocketTest, ServerSend) {
  tester_->TestServerSend();
}
