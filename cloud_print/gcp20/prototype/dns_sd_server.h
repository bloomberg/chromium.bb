// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_DNS_SD_SERVER_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_DNS_SD_SERVER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cloud_print/gcp20/prototype/service_parameters.h"
#include "net/base/ip_endpoint.h"
#include "net/udp/udp_socket.h"

namespace net {

class IOBufferWithSize;

}  // namespace net

struct DnsQueryRecord;
class DnsResponseBuilder;

// Class for sending multicast announcements, receiving queries and answering on
// them.
// TODO(maksymb): Implement probing.
class DnsSdServer : public base::SupportsWeakPtr<DnsSdServer> {
 public:
  // Constructor does not start server.
  DnsSdServer();

  // Stops the server and destroys the object.
  ~DnsSdServer();

  // Starts the server. Returns |true| if server works. Also sends
  // announcement.
  bool Start(const ServiceParameters& serv_params,
             uint32 full_ttl,
             const std::vector<std::string>& metadata) WARN_UNUSED_RESULT;

  // Sends announcement if server works.
  void Update();

  // Stops server with announcement.
  void Shutdown();

  // Returns |true| if server works.
  bool IsOnline() { return !!socket_; }

  // Updates data for TXT respond.
  void UpdateMetadata(const std::vector<std::string>& metadata);

 private:
  // Binds a socket to multicast address. Returns |true| on success.
  bool CreateSocket();

  // Processes single query.
  void ProccessQuery(uint32 current_ttl,
                     const DnsQueryRecord& query,
                     DnsResponseBuilder* builder) const;

  // Processes DNS message.
  void ProcessMessage(int len, net::IOBufferWithSize* buf);

  // CompletionCallback for receiving data from DNS.
  void DoLoop(int rv);

  // Function to start listening to socket (delegate to DoLoop function).
  void OnDatagramReceived();

  // Sends announcement.
  void SendAnnouncement(uint32 ttl);

  // Calculates and returns current TTL (with accordance to last send
  // announcement time.
  uint32 GetCurrentTLL() const;

  // Stores socket to multicast address.
  scoped_ptr<net::UDPSocket> socket_;

  // Stores multicast address end point.
  net::IPEndPoint multicast_address_;

  // Stores time until last announcement is live.
  base::Time time_until_live_;

  // Stores service parameters (like service-name and service-type etc.)
  ServiceParameters serv_params_;

  // Stores the buffer for receiving messages.
  scoped_refptr<net::IOBufferWithSize> recv_buf_;

  // Stores address from where last message was sent.
  net::IPEndPoint recv_address_;

  // Stores information for TXT respond.
  std::vector<std::string> metadata_;

  // TTL for announcements
  uint32 full_ttl_;

  DISALLOW_COPY_AND_ASSIGN(DnsSdServer);
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_DNS_SD_SERVER_H_

