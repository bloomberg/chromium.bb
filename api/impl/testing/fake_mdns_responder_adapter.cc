// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/testing/fake_mdns_responder_adapter.h"

#include <algorithm>

#include "platform/api/logging.h"

namespace openscreen {

constexpr char kLocalDomain[] = "local";

mdns::PtrEvent MakePtrEvent(const std::string& service_instance,
                            const std::string& service_type,
                            const std::string& service_protocol,
                            platform::UdpSocketPtr socket) {
  const auto labels = std::vector<std::string>{service_instance, service_type,
                                               service_protocol, kLocalDomain};
  mdns::DomainName full_instance_name;
  CHECK(mdns::DomainName::FromLabels(labels.begin(), labels.end(),
                                     &full_instance_name));
  mdns::PtrEvent result{
      mdns::QueryEventHeader{mdns::QueryEventHeader::Type::kAdded, socket},
      full_instance_name};
  return result;
}

mdns::SrvEvent MakeSrvEvent(const std::string& service_instance,
                            const std::string& service_type,
                            const std::string& service_protocol,
                            const std::string& hostname,
                            uint16_t port,
                            platform::UdpSocketPtr socket) {
  const auto instance_labels = std::vector<std::string>{
      service_instance, service_type, service_protocol, kLocalDomain};
  mdns::DomainName full_instance_name;
  CHECK(mdns::DomainName::FromLabels(
      instance_labels.begin(), instance_labels.end(), &full_instance_name));
  const auto host_labels = std::vector<std::string>{hostname, kLocalDomain};
  mdns::DomainName domain_name;
  CHECK(mdns::DomainName::FromLabels(host_labels.begin(), host_labels.end(),
                                     &domain_name));
  mdns::SrvEvent result{
      mdns::QueryEventHeader{mdns::QueryEventHeader::Type::kAdded, socket},
      full_instance_name, domain_name, port};
  return result;
}

mdns::TxtEvent MakeTxtEvent(const std::string& service_instance,
                            const std::string& service_type,
                            const std::string& service_protocol,
                            const std::vector<std::string>& txt_lines,
                            platform::UdpSocketPtr socket) {
  const auto labels = std::vector<std::string>{service_instance, service_type,
                                               service_protocol, kLocalDomain};
  mdns::DomainName full_instance_name;
  CHECK(mdns::DomainName::FromLabels(labels.begin(), labels.end(),
                                     &full_instance_name));
  mdns::TxtEvent result{
      mdns::QueryEventHeader{mdns::QueryEventHeader::Type::kAdded, socket},
      full_instance_name, txt_lines};
  return result;
}

mdns::AEvent MakeAEvent(const std::string& hostname,
                        IPAddress address,
                        platform::UdpSocketPtr socket) {
  const auto labels = std::vector<std::string>{hostname, kLocalDomain};
  mdns::DomainName domain_name;
  CHECK(
      mdns::DomainName::FromLabels(labels.begin(), labels.end(), &domain_name));
  mdns::AEvent result{
      mdns::QueryEventHeader{mdns::QueryEventHeader::Type::kAdded, socket},
      domain_name, address};
  return result;
}

mdns::AaaaEvent MakeAaaaEvent(const std::string& hostname,
                              IPAddress address,
                              platform::UdpSocketPtr socket) {
  const auto labels = std::vector<std::string>{hostname, kLocalDomain};
  mdns::DomainName domain_name;
  CHECK(
      mdns::DomainName::FromLabels(labels.begin(), labels.end(), &domain_name));
  mdns::AaaaEvent result{
      mdns::QueryEventHeader{mdns::QueryEventHeader::Type::kAdded, socket},
      domain_name, address};
  return result;
}

void AddEventsForNewService(FakeMdnsResponderAdapter* mdns_responder,
                            const std::string& service_instance,
                            const std::string& service_name,
                            const std::string& service_protocol,
                            const std::string& hostname,
                            uint16_t port,
                            const std::vector<std::string>& txt_lines,
                            const IPAddress& address,
                            platform::UdpSocketPtr socket) {
  mdns_responder->AddPtrEvent(
      MakePtrEvent(service_instance, service_name, service_protocol, socket));
  mdns_responder->AddSrvEvent(MakeSrvEvent(service_instance, service_name,
                                           service_protocol, hostname, port,
                                           socket));
  mdns_responder->AddTxtEvent(MakeTxtEvent(
      service_instance, service_name, service_protocol, txt_lines, socket));
  mdns_responder->AddAEvent(MakeAEvent(hostname, address, socket));
}

FakeMdnsResponderAdapter::~FakeMdnsResponderAdapter() = default;

void FakeMdnsResponderAdapter::AddPtrEvent(mdns::PtrEvent&& ptr_event) {
  if (running_) {
    ptr_events_.push_back(std::move(ptr_event));
  }
}

void FakeMdnsResponderAdapter::AddSrvEvent(mdns::SrvEvent&& srv_event) {
  if (running_) {
    srv_events_.push_back(std::move(srv_event));
  }
}

void FakeMdnsResponderAdapter::AddTxtEvent(mdns::TxtEvent&& txt_event) {
  if (running_) {
    txt_events_.push_back(std::move(txt_event));
  }
}

void FakeMdnsResponderAdapter::AddAEvent(mdns::AEvent&& a_event) {
  if (running_) {
    a_events_.push_back(std::move(a_event));
  }
}

void FakeMdnsResponderAdapter::AddAaaaEvent(mdns::AaaaEvent&& aaaa_event) {
  if (running_) {
    aaaa_events_.push_back(std::move(aaaa_event));
  }
}

bool FakeMdnsResponderAdapter::Init() {
  CHECK(!running_);
  running_ = true;
  return true;
}

void FakeMdnsResponderAdapter::Close() {
  ptr_queries_.clear();
  srv_queries_.clear();
  txt_queries_.clear();
  a_queries_.clear();
  ptr_events_.clear();
  srv_events_.clear();
  txt_events_.clear();
  a_events_.clear();
  registered_interfaces_.clear();
  registered_services_.clear();
  running_ = false;
}

bool FakeMdnsResponderAdapter::SetHostLabel(const std::string& host_label) {
  return false;
}

bool FakeMdnsResponderAdapter::RegisterInterface(
    const platform::InterfaceInfo& interface_info,
    const platform::IPSubnet& interface_address,
    platform::UdpSocketPtr socket) {
  if (!running_) {
    return false;
  }
  if (std::find_if(registered_interfaces_.begin(), registered_interfaces_.end(),
                   [&socket](const RegisteredInterface& interface) {
                     return interface.socket == socket;
                   }) != registered_interfaces_.end()) {
    return false;
  }
  registered_interfaces_.push_back({interface_info, interface_address, socket});
  return true;
}

bool FakeMdnsResponderAdapter::DeregisterInterface(
    platform::UdpSocketPtr socket) {
  auto it =
      std::find_if(registered_interfaces_.begin(), registered_interfaces_.end(),
                   [&socket](const RegisteredInterface& interface) {
                     return interface.socket == socket;
                   });
  if (it == registered_interfaces_.end()) {
    return false;
  }
  registered_interfaces_.erase(it);
  return true;
}

void FakeMdnsResponderAdapter::OnDataReceived(
    const IPEndpoint& source,
    const IPEndpoint& original_destination,
    const uint8_t* data,
    size_t length,
    platform::UdpSocketPtr receiving_socket) {
  CHECK(false) << "Tests should not drive this class with packets";
}

int FakeMdnsResponderAdapter::RunTasks() {
  return 1;
}

std::vector<mdns::AEvent> FakeMdnsResponderAdapter::TakeAResponses() {
  const auto query_it = std::stable_partition(
      a_events_.begin(), a_events_.end(), [this](const mdns::AEvent& a_event) {
        for (const auto& query : a_queries_) {
          if (a_event.domain_name == query) {
            return false;
          }
        }
        return true;
      });
  std::vector<mdns::AEvent> result;
  for (auto it = query_it; it != a_events_.end(); ++it) {
    result.push_back(std::move(*it));
  }
  LOG_INFO << "taking " << result.size() << " a response(s)";
  a_events_.erase(query_it, a_events_.end());
  return result;
}

std::vector<mdns::AaaaEvent> FakeMdnsResponderAdapter::TakeAaaaResponses() {
  const auto query_it =
      std::stable_partition(aaaa_events_.begin(), aaaa_events_.end(),
                            [this](const mdns::AaaaEvent& aaaa_event) {
                              for (const auto& query : aaaa_queries_) {
                                if (aaaa_event.domain_name == query) {
                                  return false;
                                }
                              }
                              return true;
                            });
  std::vector<mdns::AaaaEvent> result;
  for (auto it = query_it; it != aaaa_events_.end(); ++it) {
    result.push_back(std::move(*it));
  }
  LOG_INFO << "taking " << result.size() << " a response(s)";
  aaaa_events_.erase(query_it, aaaa_events_.end());
  return result;
}

std::vector<mdns::PtrEvent> FakeMdnsResponderAdapter::TakePtrResponses() {
  const auto query_it = std::stable_partition(
      ptr_events_.begin(), ptr_events_.end(),
      [this](const mdns::PtrEvent& ptr_event) {
        const auto instance_labels = ptr_event.service_instance.GetLabels();
        for (const auto& query : ptr_queries_) {
          const auto query_labels = query.GetLabels();
          // TODO(btolsch): Just use qname if it's added to PtrEvent.
          if (std::equal(instance_labels.begin() + 1, instance_labels.end(),
                         query_labels.begin())) {
            return false;
          }
        }
        return true;
      });
  std::vector<mdns::PtrEvent> result;
  for (auto it = query_it; it != ptr_events_.end(); ++it) {
    result.push_back(std::move(*it));
  }
  LOG_INFO << "taking " << result.size() << " ptr response(s)";
  ptr_events_.erase(query_it, ptr_events_.end());
  return result;
}

std::vector<mdns::SrvEvent> FakeMdnsResponderAdapter::TakeSrvResponses() {
  const auto query_it =
      std::stable_partition(srv_events_.begin(), srv_events_.end(),
                            [this](const mdns::SrvEvent& srv_event) {
                              for (const auto& query : srv_queries_) {
                                if (srv_event.service_instance == query) {
                                  return false;
                                }
                              }
                              return true;
                            });
  std::vector<mdns::SrvEvent> result;
  for (auto it = query_it; it != srv_events_.end(); ++it) {
    result.push_back(std::move(*it));
  }
  LOG_INFO << "taking " << result.size() << " srv response(s)";
  srv_events_.erase(query_it, srv_events_.end());
  return result;
}

std::vector<mdns::TxtEvent> FakeMdnsResponderAdapter::TakeTxtResponses() {
  const auto query_it =
      std::stable_partition(txt_events_.begin(), txt_events_.end(),
                            [this](const mdns::TxtEvent& txt_event) {
                              for (const auto& query : txt_queries_) {
                                if (txt_event.service_instance == query) {
                                  return false;
                                }
                              }
                              return true;
                            });
  std::vector<mdns::TxtEvent> result;
  for (auto it = query_it; it != txt_events_.end(); ++it) {
    result.push_back(std::move(*it));
  }
  LOG_INFO << "taking " << result.size() << " txt response(s)";
  txt_events_.erase(query_it, txt_events_.end());
  return result;
}

mdns::MdnsResponderErrorCode FakeMdnsResponderAdapter::StartAQuery(
    const mdns::DomainName& domain_name) {
  if (!running_) {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
  auto maybe_inserted = a_queries_.insert(domain_name);
  if (maybe_inserted.second) {
    return mdns::MdnsResponderErrorCode::kNoError;
  } else {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
}

mdns::MdnsResponderErrorCode FakeMdnsResponderAdapter::StartAaaaQuery(
    const mdns::DomainName& domain_name) {
  if (!running_) {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
  auto maybe_inserted = aaaa_queries_.insert(domain_name);
  if (maybe_inserted.second) {
    return mdns::MdnsResponderErrorCode::kNoError;
  } else {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
}

mdns::MdnsResponderErrorCode FakeMdnsResponderAdapter::StartPtrQuery(
    const mdns::DomainName& service_type) {
  if (!running_) {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
  auto canonical_service_type = service_type;
  if (!canonical_service_type.EndsWithLocalDomain()) {
    CHECK(canonical_service_type.Append(mdns::DomainName::kLocalDomain));
  }
  auto maybe_inserted = ptr_queries_.insert(canonical_service_type);
  if (maybe_inserted.second) {
    return mdns::MdnsResponderErrorCode::kNoError;
  } else {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
}

mdns::MdnsResponderErrorCode FakeMdnsResponderAdapter::StartSrvQuery(
    const mdns::DomainName& service_instance) {
  if (!running_) {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
  auto maybe_inserted = srv_queries_.insert(service_instance);
  if (maybe_inserted.second) {
    return mdns::MdnsResponderErrorCode::kNoError;
  } else {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
}

mdns::MdnsResponderErrorCode FakeMdnsResponderAdapter::StartTxtQuery(
    const mdns::DomainName& service_instance) {
  if (!running_) {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
  auto maybe_inserted = txt_queries_.insert(service_instance);
  if (maybe_inserted.second) {
    return mdns::MdnsResponderErrorCode::kNoError;
  } else {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
}

mdns::MdnsResponderErrorCode FakeMdnsResponderAdapter::StopAQuery(
    const mdns::DomainName& domain_name) {
  auto it = a_queries_.find(domain_name);
  if (it == a_queries_.end()) {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
  a_queries_.erase(it);
  return mdns::MdnsResponderErrorCode::kNoError;
}

mdns::MdnsResponderErrorCode FakeMdnsResponderAdapter::StopAaaaQuery(
    const mdns::DomainName& domain_name) {
  auto it = aaaa_queries_.find(domain_name);
  if (it == aaaa_queries_.end()) {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
  aaaa_queries_.erase(it);
  return mdns::MdnsResponderErrorCode::kNoError;
}

mdns::MdnsResponderErrorCode FakeMdnsResponderAdapter::StopPtrQuery(
    const mdns::DomainName& service_type) {
  auto canonical_service_type = service_type;
  if (!canonical_service_type.EndsWithLocalDomain()) {
    CHECK(canonical_service_type.Append(mdns::DomainName::kLocalDomain));
  }
  auto it = ptr_queries_.find(canonical_service_type);
  if (it == ptr_queries_.end()) {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
  ptr_queries_.erase(it);
  return mdns::MdnsResponderErrorCode::kNoError;
}

mdns::MdnsResponderErrorCode FakeMdnsResponderAdapter::StopSrvQuery(
    const mdns::DomainName& service_instance) {
  auto it = srv_queries_.find(service_instance);
  if (it == srv_queries_.end()) {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
  srv_queries_.erase(it);
  return mdns::MdnsResponderErrorCode::kNoError;
}

mdns::MdnsResponderErrorCode FakeMdnsResponderAdapter::StopTxtQuery(
    const mdns::DomainName& service_instance) {
  auto it = txt_queries_.find(service_instance);
  if (it == txt_queries_.end()) {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
  txt_queries_.erase(it);
  return mdns::MdnsResponderErrorCode::kNoError;
}

mdns::MdnsResponderErrorCode FakeMdnsResponderAdapter::RegisterService(
    const std::string& service_instance,
    const std::string& service_name,
    const std::string& service_protocol,
    const mdns::DomainName& target_host,
    uint16_t target_port,
    const std::vector<std::string>& lines) {
  if (!running_) {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
  if (std::find_if(registered_services_.begin(), registered_services_.end(),
                   [&service_instance, &service_name,
                    &service_protocol](const RegisteredService& service) {
                     return service.service_instance == service_instance &&
                            service.service_name == service_name &&
                            service.service_protocol == service_protocol;
                   }) != registered_services_.end()) {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
  registered_services_.push_back({service_instance, service_name,
                                  service_protocol, target_host, target_port,
                                  lines});
  return mdns::MdnsResponderErrorCode::kNoError;
}

mdns::MdnsResponderErrorCode FakeMdnsResponderAdapter::DeregisterService(
    const std::string& service_instance,
    const std::string& service_name,
    const std::string& service_protocol) {
  if (!running_) {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
  auto it =
      std::find_if(registered_services_.begin(), registered_services_.end(),
                   [&service_instance, &service_name,
                    &service_protocol](const RegisteredService& service) {
                     return service.service_instance == service_instance &&
                            service.service_name == service_name &&
                            service.service_protocol == service_protocol;
                   });
  if (it == registered_services_.end()) {
    return mdns::MdnsResponderErrorCode::kUnknownError;
  }
  registered_services_.erase(it);
  return mdns::MdnsResponderErrorCode::kNoError;
}

}  // namespace openscreen
