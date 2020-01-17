// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_SERVICE_IMPL_H_
#define DISCOVERY_MDNS_MDNS_SERVICE_IMPL_H_

#include "discovery/mdns/mdns_domain_confirmed_provider.h"
#include "discovery/mdns/mdns_probe_manager.h"
#include "discovery/mdns/mdns_publisher.h"
#include "discovery/mdns/mdns_querier.h"
#include "discovery/mdns/mdns_random.h"
#include "discovery/mdns/mdns_reader.h"
#include "discovery/mdns/mdns_receiver.h"
#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/mdns_responder.h"
#include "discovery/mdns/mdns_sender.h"
#include "discovery/mdns/mdns_writer.h"
#include "discovery/mdns/public/mdns_constants.h"
#include "discovery/mdns/public/mdns_service.h"
#include "platform/api/udp_socket.h"

namespace openscreen {
namespace discovery {

class MdnsServiceImpl : public MdnsService {
 public:
  class ReceiverClient : public UdpSocket::Client {
   public:
    // |receiver| must persist until this class is destroyed.
    void SetReceiver(MdnsReceiver* receiver);

    // UdpSocket::Client overrides.
    void OnError(UdpSocket* socket, Error error) override;
    void OnSendError(UdpSocket* socket, Error error) override;
    void OnRead(UdpSocket* socket, ErrorOr<UdpPacket> packet) override;

   private:
    MdnsReceiver* receiver_ = nullptr;
  };

  MdnsServiceImpl(TaskRunner* task_runner,
                  ClockNowFunctionPtr now_function,
                  std::unique_ptr<ReceiverClient> callback,
                  std::unique_ptr<UdpSocket> socket);

  void StartQuery(const DomainName& name,
                  DnsType dns_type,
                  DnsClass dns_class,
                  MdnsRecordChangedCallback* callback) override;

  void StopQuery(const DomainName& name,
                 DnsType dns_type,
                 DnsClass dns_class,
                 MdnsRecordChangedCallback* callback) override;

  void ReinitializeQueries(const DomainName& name) override;

  Error StartProbe(MdnsDomainConfirmedProvider* callback,
                   DomainName requested_name,
                   IPAddress address) override;

  Error RegisterRecord(const MdnsRecord& record) override;

  Error UpdateRegisteredRecord(const MdnsRecord& old_record,
                               const MdnsRecord& new_record) override;

  Error UnregisterRecord(const MdnsRecord& record) override;

 private:
  TaskRunner* const task_runner_;
  ClockNowFunctionPtr now_function_;
  std::unique_ptr<ReceiverClient> callback_;
  std::unique_ptr<UdpSocket> socket_;

  MdnsRandom random_delay_;
  MdnsSender sender_;
  MdnsReceiver receiver_;
  MdnsQuerier querier_;
  MdnsProbeManagerImpl probe_manager_;
  MdnsPublisher publisher_;
  MdnsResponder responder_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_SERVICE_IMPL_H_
