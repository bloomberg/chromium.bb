// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "base/bind.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/sync_socket.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/port_server.h"
#include "net/base/sys_addrinfo.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_LINUX)
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif

namespace {

void SetOnCall(bool* called) {
  *called = true;
}

}  // namespace

TEST(PortReservationTest, Normal) {
  bool called = false;
  {
    PortReservation r(base::Bind(&SetOnCall, &called), 100);
  }
  ASSERT_TRUE(called);
}

TEST(PortReservationTest, Leak) {
  bool called = false;
  {
    PortReservation r(base::Bind(&SetOnCall, &called), 100);
    r.Leak();
  }
  ASSERT_FALSE(called);
}

TEST(PortReservationTest, MultipleLeaks) {
  bool called = false;
  {
    PortReservation r(base::Bind(&SetOnCall, &called), 100);
    r.Leak();
    r.Leak();
  }
  ASSERT_FALSE(called);
}

#if defined(OS_LINUX)
namespace {

void RunServerOnThread(const std::string& path,
                       const std::string& response,
                       base::WaitableEvent* listen_event,
                       std::string* request) {
  int server_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_GE(server_sock_fd, 0);
  ASSERT_GE(fcntl(server_sock_fd, F_SETFL, O_NONBLOCK), 0);
  base::SyncSocket server_sock(server_sock_fd);

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  memcpy(addr.sun_path, &path[0], path.length());
  ASSERT_EQ(0,
            bind(server_sock_fd,
                 reinterpret_cast<struct sockaddr*>(&addr),
                 sizeof(sa_family_t) + path.length()));
  ASSERT_EQ(0, listen(server_sock_fd, 1));
  listen_event->Signal();

  struct sockaddr_un  cli_addr;
  socklen_t clilen = sizeof(cli_addr);
  base::TimeTicks deadline =
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(2);
  int client_sock_fd = -1;
  while (base::TimeTicks::Now() < deadline && client_sock_fd < 0) {
    client_sock_fd = accept(
        server_sock_fd, reinterpret_cast<struct sockaddr*>(&cli_addr), &clilen);
  }
  ASSERT_GE(client_sock_fd, 0);
  base::SyncSocket sock(client_sock_fd);
  do {
    char c = 0;
    size_t rv = sock.Receive(&c, 1);
    if (!rv)
      break;
    request->push_back(c);
  } while (sock.Peek());
  sock.Send(response.c_str(), response.length());
}

std::string GenerateRandomPath() {
  std::string path = base::GenerateGUID();
  if (!path.empty()) {
    std::string pre_path;
    pre_path.push_back(0);  // Linux abstract namespace.
    path = pre_path + path;
  }
  return path;
}

}  // namespace

class PortServerTest : public testing::Test {
 public:
  PortServerTest() : thread_("server") {
    EXPECT_TRUE(thread_.Start());
  }

  void RunServer(const std::string& path,
                 const std::string& response,
                 std::string* request) {
    base::WaitableEvent listen_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&RunServerOnThread, path, response,
                                  &listen_event, request));
    ASSERT_TRUE(listen_event.TimedWait(base::TimeDelta::FromSeconds(5)));
  }

 private:
  base::Thread thread_;
};

TEST_F(PortServerTest, Reserve) {
  std::string path = GenerateRandomPath();
  PortServer server(path);

  std::string request;
  RunServer(path, "12345\n", &request);

  uint16_t port = 0;
  std::unique_ptr<PortReservation> reservation;
  Status status = server.ReservePort(&port, &reservation);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(12345u, port);
}

TEST_F(PortServerTest, ReserveResetReserve) {
  std::string path = GenerateRandomPath();
  PortServer server(path);

  std::string request;
  RunServer(path, "12345\n", &request);

  uint16_t port = 0;
  std::unique_ptr<PortReservation> reservation;
  Status status = server.ReservePort(&port, &reservation);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(12345u, port);

  reservation.reset();
  status = server.ReservePort(&port, &reservation);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(12345u, port);
}

TEST_F(PortServerTest, ReserveReserve) {
  std::string path = GenerateRandomPath();
  PortServer server(path);

  std::string request;
  RunServer(path, "12345\n", &request);

  uint16_t port = 0;
  std::unique_ptr<PortReservation> reservation;
  Status status = server.ReservePort(&port, &reservation);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(12345u, port);

  RunServer(path, "12346\n", &request);
  status = server.ReservePort(&port, &reservation);
  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(12346u, port);
}
#endif

TEST(PortManagerTest, ReservePort) {
  PortManager mgr(15000, 16000);
  uint16_t port = 0;
  std::unique_ptr<PortReservation> reservation;
  Status status = mgr.ReservePort(&port, &reservation);
  ASSERT_EQ(kOk, status.code()) << status.message();

  ASSERT_GE(port, 15000);
  ASSERT_LE(port, 16000);
  ASSERT_TRUE(reservation);
}

TEST(PortManagerTest, ReservePortFromPool) {
  PortManager mgr(15000, 16000);
  uint16_t first_port = 0, port = 1;
  for (int i = 0; i < 10; i++) {
    std::unique_ptr<PortReservation> reservation;
    Status status = mgr.ReservePortFromPool(&port, &reservation);
    ASSERT_EQ(kOk, status.code()) << status.message();
    ASSERT_TRUE(reservation);
    ASSERT_GE(port, 15000);
    ASSERT_LE(port, 16000);
    if (i == 0)
      first_port = port;
    ASSERT_EQ(port, first_port);
  }
}
