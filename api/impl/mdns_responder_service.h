// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_MDNS_RESPONDER_SERVICE_H_
#define API_IMPL_MDNS_RESPONDER_SERVICE_H_

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/impl/mdns_platform_service.h"
#include "api/impl/screen_listener_impl.h"
#include "api/impl/screen_publisher_impl.h"
#include "base/ip_address.h"
#include "discovery/mdns/mdns_responder_adapter.h"
#include "platform/base/event_loop.h"

namespace openscreen {

class MdnsResponderAdapterFactory {
 public:
  virtual ~MdnsResponderAdapterFactory() = default;

  virtual std::unique_ptr<mdns::MdnsResponderAdapter> Create() = 0;
};

class MdnsResponderService final : public ScreenListenerImpl::Delegate,
                                   public ScreenPublisherImpl::Delegate {
 public:
  explicit MdnsResponderService(
      const std::string& service_name,
      const std::string& service_protocol,
      std::unique_ptr<MdnsResponderAdapterFactory> mdns_responder_factory,
      std::unique_ptr<MdnsPlatformService> platform);
  ~MdnsResponderService() override;

  void SetServiceConfig(const std::string& hostname,
                        const std::string& instance,
                        uint16_t port,
                        const std::vector<int32_t> interface_index_whitelist,
                        const std::vector<std::string>& txt_lines);

  void HandleNewEvents(const std::vector<platform::ReceivedData>& data);

  // ScreenListenerImpl::Delegate overrides.
  void StartListener() override;
  void StartAndSuspendListener() override;
  void StopListener() override;
  void SuspendListener() override;
  void ResumeListener() override;
  void SearchNow(ScreenListener::State from) override;

  // ScreenPublisherImpl::Delegate overrides.
  void StartPublisher() override;
  void StartAndSuspendPublisher() override;
  void StopPublisher() override;
  void SuspendPublisher() override;
  void ResumePublisher() override;
  void UpdateFriendlyName(const std::string& friendly_name) override;

 private:
  // NOTE: service_instance implicit in map key.
  struct ServiceInstance {
    int32_t ptr_interface_index = 0;
    mdns::DomainName domain_name;
    uint16_t port = 0;
    std::vector<std::string> txt_info;
  };

  // NOTE: hostname implicit in map key.
  struct HostnameWatchers {
    std::vector<ServiceInstance*> services;
    IPAddress address;
  };

  void HandleMdnsEvents();
  void StartListening();
  void StopListening();
  void StartService();
  void StopService();
  void StopMdnsResponder();
  void PushScreenInfo(const mdns::DomainName& service_instance,
                      const ServiceInstance& instance_info,
                      const IPAddress& address);
  void MaybePushScreenInfo(const mdns::DomainName& service_instance,
                           const ServiceInstance& instance_info);
  void MaybePushScreenInfo(const mdns::DomainName& domain_name,
                           const IPAddress& address);
  void RemoveScreenInfo(const mdns::DomainName& service_instance);
  void RemoveScreenInfoByDomain(const mdns::DomainName& domain_name);
  void RemoveAllScreens();

  bool HandlePtrEvent(const mdns::PtrEvent& ptr_event);
  bool HandleSrvEvent(const mdns::SrvEvent& srv_event);
  bool HandleTxtEvent(const mdns::TxtEvent& txt_event);
  bool HandleAEvent(const mdns::AEvent& a_event);
  bool HandleAaaaEvent(const mdns::AaaaEvent& aaaa_event);

  // Service type separated as service name and service protocol for both
  // listening and publishing (e.g. {"_openscreen", "_udp"}).
  std::array<std::string, 2> service_type_;

  // The following variables all relate to what MdnsResponderService publishes,
  // if anything.
  std::string service_hostname_;
  std::string service_instance_name_;
  uint16_t service_port_;
  std::vector<int32_t> interface_index_whitelist_;
  std::vector<std::string> service_txt_lines_;

  std::unique_ptr<MdnsResponderAdapterFactory> mdns_responder_factory_;
  std::unique_ptr<mdns::MdnsResponderAdapter> mdns_responder_;
  std::unique_ptr<MdnsPlatformService> platform_;
  std::vector<MdnsPlatformService::BoundInterface> bound_interfaces_;

  // A map of service information collected from PTR, SRV, and TXT records.  It
  // is keyed by service instance names.
  std::map<mdns::DomainName,
           std::unique_ptr<ServiceInstance>,
           mdns::DomainNameComparator>
      services_;

  // A map of hostnames to IPAddresses which also includes pointers to dependent
  // service instances.  The service instance pointers act as a reference count
  // to keep the A/AAAA queries alive, when more than one service refers to the
  // same hostname.  This is not currently used by openscreen, but is used by
  // Cast, so may be supported in openscreen in the future.
  std::map<mdns::DomainName, HostnameWatchers, mdns::DomainNameComparator>
      hostname_watchers_;

  std::map<std::string, ScreenInfo> screen_info_;
};

}  // namespace openscreen

#endif  // API_IMPL_MDNS_RESPONDER_SERVICE_H_
