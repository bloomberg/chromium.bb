// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_PORT_SERVER_H_
#define CHROME_TEST_CHROMEDRIVER_NET_PORT_SERVER_H_

#include <list>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"

class Status;

class PortReservation {
 public:
  PortReservation(const base::Closure& on_free_func, int port);
  ~PortReservation();

  void Leak();

 private:
  base::Closure on_free_func_;
  int port_;
};

// Communicates with a port reservation management server.
class PortServer {
 public:
  // Construct a port server that communicates via the unix domain socket with
  // the given path. Must use the Linux abstract namespace.
  explicit PortServer(const std::string& path);
  ~PortServer();

  Status ReservePort(int* port, scoped_ptr<PortReservation>* reservation);

 private:
  Status RequestPort(int* port);
  void ReleasePort(int port);

  std::string path_;

  base::Lock free_lock_;
  std::list<int> free_;
};

bool SetSocketTimeout(const base::TimeDelta& timeout);

#endif  // CHROME_TEST_CHROMEDRIVER_NET_PORT_SERVER_H_
