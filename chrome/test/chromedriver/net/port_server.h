// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_PORT_SERVER_H_
#define CHROME_TEST_CHROMEDRIVER_NET_PORT_SERVER_H_

#include <list>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"

class Status;

class PortReservation {
 public:
  PortReservation(const base::Closure& on_free_func, uint16 port);
  ~PortReservation();

  void Leak();

 private:
  base::Closure on_free_func_;
  uint16 port_;
};

// Communicates with a port reservation management server.
class PortServer {
 public:
  // Construct a port server that communicates via the unix domain socket with
  // the given path. Must use the Linux abstract namespace.
  explicit PortServer(const std::string& path);
  ~PortServer();

  Status ReservePort(uint16* port, scoped_ptr<PortReservation>* reservation);

 private:
  Status RequestPort(uint16* port);
  void ReleasePort(uint16 port);

  std::string path_;

  base::Lock free_lock_;
  std::list<uint16> free_;
};

// Manages reservation of a block of local ports.
class PortManager {
 public:
  PortManager(uint16 min_port, uint16 max_port);
  ~PortManager();

  Status ReservePort(uint16* port, scoped_ptr<PortReservation>* reservation);
  // Since we cannot remove forwarded adb ports on older SDKs,
  // maintain a pool of forwarded ports for reuse.
  Status ReservePortFromPool(uint16* port,
                             scoped_ptr<PortReservation>* reservation);

 private:
  uint16 FindAvailablePort() const;
  void ReleasePort(uint16 port);
  void ReleasePortToPool(uint16 port);

  base::Lock lock_;
  std::set<uint16> taken_;
  std::list<uint16> unused_forwarded_port_;
  uint16 min_port_;
  uint16 max_port_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_PORT_SERVER_H_
