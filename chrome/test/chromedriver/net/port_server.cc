// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/port_server.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/sync_socket.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "net/base/sys_addrinfo.h"

#if defined(OS_LINUX)
#include <sys/socket.h>
#include <sys/un.h>
#endif

PortReservation::PortReservation(const base::Closure& on_free_func, int port)
    : on_free_func_(on_free_func), port_(port) {}

PortReservation::~PortReservation() {
  if (!on_free_func_.is_null())
    on_free_func_.Run();
}

void PortReservation::Leak() {
  LOG(ERROR) << "Port leaked: " << port_;
  on_free_func_.Reset();
}

PortServer::PortServer(const std::string& path) : path_(path) {
  CHECK(path_.size() && path_[0] == 0)
      << "path must be for Linux abstract namespace";
}

PortServer::~PortServer() {}

Status PortServer::ReservePort(int* port,
                               scoped_ptr<PortReservation>* reservation) {
  int port_to_use = 0;
  {
    base::AutoLock lock(free_lock_);
    if (free_.size()) {
      port_to_use = free_.front();
      free_.pop_front();
    }
  }
  if (!port_to_use) {
    Status status = RequestPort(&port_to_use);
    if (status.IsError())
      return status;
  }
  *port = port_to_use;
  reservation->reset(new PortReservation(
      base::Bind(&PortServer::ReleasePort, base::Unretained(this), port_to_use),
      port_to_use));
  return Status(kOk);
}

Status PortServer::RequestPort(int* port) {
  // The client sends its PID + \n, and the server responds with a port + \n,
  // which is valid for the lifetime of the referred process.
#if defined(OS_LINUX)
  int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock_fd < 0)
    return Status(kUnknownError, "unable to create socket");
  base::SyncSocket sock(sock_fd);
  struct timeval tv;
  tv.tv_sec = 10;
  tv.tv_usec = 0;
  if (setsockopt(sock_fd,
                 SOL_SOCKET,
                 SO_RCVTIMEO,
                 reinterpret_cast<char*>(&tv),
                 sizeof(tv)) < 0 ||
      setsockopt(sock_fd,
                 SOL_SOCKET,
                 SO_SNDTIMEO,
                 reinterpret_cast<char*>(&tv),
                 sizeof(tv)) < 0) {
    return Status(kUnknownError, "unable to set socket timeout");
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  memcpy(addr.sun_path, &path_[0], path_.length());
  if (connect(sock.handle(),
              reinterpret_cast<struct sockaddr*>(&addr),
              sizeof(sa_family_t) + path_.length())) {
    return Status(kUnknownError, "unable to connect");
  }

  int proc_id = static_cast<int>(base::GetCurrentProcId());
  std::string request = base::IntToString(proc_id);
  request += "\n";
  VLOG(0) << "PORTSERVER REQUEST " << request;
  if (sock.Send(request.c_str(), request.length()) != request.length())
    return Status(kUnknownError, "failed to send portserver request");

  std::string response;
  do {
    char c = 0;
    size_t rv = sock.Receive(&c, 1);
    if (!rv)
      break;
    response.push_back(c);
  } while (sock.Peek());
  if (response.empty())
    return Status(kUnknownError, "failed to receive portserver response");
  VLOG(0) << "PORTSERVER RESPONSE " << response;

  int new_port = 0;
  if (*response.rbegin() != '\n' ||
      !base::StringToInt(response.substr(0, response.length() - 1), &new_port))
    return Status(kUnknownError, "failed to parse portserver response");
  *port = new_port;
  return Status(kOk);
#else
  return Status(kUnknownError, "not implemented for this platform");
#endif
}

void PortServer::ReleasePort(int port) {
  base::AutoLock lock(free_lock_);
  free_.push_back(port);
}
