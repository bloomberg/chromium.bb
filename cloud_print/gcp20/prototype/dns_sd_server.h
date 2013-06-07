// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GCP20_PROTOTYPE_DNS_SD_H_
#define GCP20_PROTOTYPE_DNS_SD_H_

#include <vector>

#include "net/dns/dns_protocol.h"
#include "net/udp/udp_socket.h"

// Class for sending multicast announcements, receiving queries and answering on
// them. Client should call |ProccessMessages| periodically to make server work.
class DnsSdServer {
 public:
  // Constructs unstarted server.
  DnsSdServer();

  // Stops server.
  ~DnsSdServer();

  // Starts the server. Returns |true| if server works. Also sends
  // announcement.
  bool Start();

  // Sends announcement if server works.
  void Update();

  // Stops server with announcement.
  void Shutdown();

  // Process pending queries for the server.
  void ProcessMessages();

  // Returns |true| if server works.
  bool is_online() { return is_online_; }

 private:
  // Binds a socket to multicast address. Returns |true| on success.
  bool CreateSocket();

  // Sends announcement.
  void SendAnnouncement(uint32 ttl);

  // Returns |true| if server received some questions.
  bool CheckPendingQueries();

  // Stores |true| if server was started.
  bool is_online_;

  // Stores socket to multicast address.
  scoped_ptr<net::UDPSocket> socket_;

  // Stores multicast address end point.
  net::IPEndPoint multicast_address_;

  DISALLOW_COPY_AND_ASSIGN(DnsSdServer);
};

#endif  // GCP20_PROTOTYPE_DNS_SD_H_

