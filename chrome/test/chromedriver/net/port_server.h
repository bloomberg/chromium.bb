// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_PORT_SERVER_H_
#define CHROME_TEST_CHROMEDRIVER_NET_PORT_SERVER_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/synchronization/lock.h"

class Status;

class PortReservation {
 public:
  PortReservation(const base::Closure& on_free_func, uint16_t port);
  ~PortReservation();

  void Leak();

 private:
  base::Closure on_free_func_;
  uint16_t port_;
};

// Communicates with a port reservation management server.
class PortServer {
 public:
  // Construct a port server that communicates via the unix domain socket with
  // the given path. Must use the Linux abstract namespace.
  explicit PortServer(const std::string& path);
  ~PortServer();

  Status ReservePort(uint16_t* port,
                     std::unique_ptr<PortReservation>* reservation);

 private:
  Status RequestPort(uint16_t* port);
  void ReleasePort(uint16_t port);

  std::string path_;

  base::Lock free_lock_;
  std::list<uint16_t> free_;
};

// Manages reservation of a block of local ports.
class PortManager {
 public:
  PortManager(uint16_t min_port, uint16_t max_port);
  ~PortManager();

  Status ReservePort(uint16_t* port,
                     std::unique_ptr<PortReservation>* reservation);
  // Since we cannot remove forwarded adb ports on older SDKs,
  // maintain a pool of forwarded ports for reuse.
  Status ReservePortFromPool(uint16_t* port,
                             std::unique_ptr<PortReservation>* reservation);

 private:
  uint16_t FindAvailablePort() const;
  void ReleasePort(uint16_t port);
  void ReleasePortToPool(uint16_t port);

  base::Lock lock_;
  std::set<uint16_t> taken_;
  std::list<uint16_t> unused_forwarded_port_;
  uint16_t min_port_;
  uint16_t max_port_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_PORT_SERVER_H_
