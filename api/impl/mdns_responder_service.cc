// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/mdns_responder_service.h"

#include <algorithm>

#include "base/make_unique.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace {

std::string ScreenIdFromServiceInstance(
    const mdns::DomainName& service_instance) {
  std::string screen_id;
  screen_id.assign(
      reinterpret_cast<const char*>(service_instance.domain_name().data()),
      service_instance.domain_name().size());
  return screen_id;
}

}  // namespace

MdnsResponderService::MdnsResponderService(
    const std::string& service_name,
    const std::string& service_protocol,
    std::unique_ptr<MdnsResponderAdapterFactory> mdns_responder_factory,
    std::unique_ptr<MdnsPlatformService> platform)
    : service_type_{service_name, service_protocol},
      mdns_responder_factory_(std::move(mdns_responder_factory)),
      platform_(std::move(platform)) {}

MdnsResponderService::~MdnsResponderService() = default;

void MdnsResponderService::SetServiceConfig(
    const std::string& hostname,
    const std::string& instance,
    uint16_t port,
    const std::vector<int32_t> interface_index_whitelist,
    const std::vector<std::string>& txt_lines) {
  DCHECK(!hostname.empty());
  DCHECK(!instance.empty());
  DCHECK_NE(0, port);
  service_hostname_ = hostname;
  service_instance_name_ = instance;
  service_port_ = port;
  interface_index_whitelist_ = interface_index_whitelist;
  service_txt_lines_ = txt_lines;
}

void MdnsResponderService::HandleNewEvents(
    const std::vector<platform::ReceivedData>& data) {
  if (!mdns_responder_)
    return;
  for (auto& packet : data) {
    mdns_responder_->OnDataReceived(packet.source, packet.original_destination,
                                    packet.bytes.data(), packet.length,
                                    packet.socket);
  }
  mdns_responder_->RunTasks();

  HandleMdnsEvents();
}

void MdnsResponderService::StartListener() {
  if (!mdns_responder_) {
    mdns_responder_ = mdns_responder_factory_->Create();
  }
  StartListening();
  ScreenListenerImpl::Delegate::SetState(ScreenListener::State::kRunning);
}

void MdnsResponderService::StartAndSuspendListener() {
  mdns_responder_ = mdns_responder_factory_->Create();
  ScreenListenerImpl::Delegate::SetState(ScreenListener::State::kSuspended);
}

void MdnsResponderService::StopListener() {
  StopListening();
  if (!publisher_ || publisher_->state() == ScreenPublisher::State::kStopped) {
    StopMdnsResponder();
    mdns_responder_.reset();
  }
  ScreenListenerImpl::Delegate::SetState(ScreenListener::State::kStopped);
}

void MdnsResponderService::SuspendListener() {
  // TODO(btolsch): What state should suspend actually produce for both
  // listening and publishing?  The issue is around |mdns_responder_| and
  // whether it needs to be destroyed once we stop obeying its Execute
  // scheduling requirement.  We might be able to flush the cache on resume or
  // maybe mDNSResponder handles this anyway.
  StopMdnsResponder();
  ScreenListenerImpl::Delegate::SetState(ScreenListener::State::kSuspended);
}

void MdnsResponderService::ResumeListener() {
  StartListening();
  ScreenListenerImpl::Delegate::SetState(ScreenListener::State::kRunning);
}

void MdnsResponderService::SearchNow(ScreenListener::State from) {
  ScreenListenerImpl::Delegate::SetState(from);
}

void MdnsResponderService::StartPublisher() {
  if (!mdns_responder_) {
    mdns_responder_ = mdns_responder_factory_->Create();
  }
  StartService();
  ScreenPublisherImpl::Delegate::SetState(ScreenPublisher::State::kRunning);
}

void MdnsResponderService::StartAndSuspendPublisher() {
  mdns_responder_ = mdns_responder_factory_->Create();
  ScreenPublisherImpl::Delegate::SetState(ScreenPublisher::State::kSuspended);
}

void MdnsResponderService::StopPublisher() {
  StopService();
  // TODO(btolsch): We could also only call StopMdnsResponder for
  // ScreenListenerState::kSuspended.  If these ever become asynchronous
  // (unlikely), we'd also have to consider kStarting/kStopping/etc.  Similar
  // comments apply to StopListener.
  if (!listener_ || listener_->state() == ScreenListener::State::kStopped) {
    StopMdnsResponder();
    mdns_responder_.reset();
  }
  ScreenPublisherImpl::Delegate::SetState(ScreenPublisher::State::kStopped);
}

void MdnsResponderService::SuspendPublisher() {
  StopService();
  ScreenPublisherImpl::Delegate::SetState(ScreenPublisher::State::kSuspended);
}

void MdnsResponderService::ResumePublisher() {
  StartService();
  ScreenPublisherImpl::Delegate::SetState(ScreenPublisher::State::kRunning);
}

void MdnsResponderService::UpdateFriendlyName(
    const std::string& friendly_name) {
  // TODO(btolsch): Need an MdnsResponderAdapter method to update the TXT
  // record.
}

void MdnsResponderService::HandleMdnsEvents() {
  // NOTE: In the worst case, we might get a single combined packet for
  // PTR/SRV/TXT and then no other packets.  If we don't loop here, we would
  // start SRV/TXT queries based on the PTR response, but never check for events
  // again.  This should no longer be a problem when we have correct scheduling
  // of RunTasks.
  bool events_possible = false;
  do {
    events_possible = false;
    for (auto& ptr_event : mdns_responder_->TakePtrResponses()) {
      events_possible = HandlePtrEvent(ptr_event) || events_possible;
    }
    for (auto& srv_event : mdns_responder_->TakeSrvResponses()) {
      events_possible = HandleSrvEvent(srv_event) || events_possible;
    }
    for (auto& txt_event : mdns_responder_->TakeTxtResponses()) {
      events_possible = HandleTxtEvent(txt_event) || events_possible;
    }
    for (const auto& a_event : mdns_responder_->TakeAResponses()) {
      events_possible = HandleAEvent(a_event) || events_possible;
    }
    for (const auto& aaaa_event : mdns_responder_->TakeAaaaResponses()) {
      events_possible = HandleAaaaEvent(aaaa_event) || events_possible;
    }
    if (events_possible) {
      mdns_responder_->RunTasks();
    }
  } while (events_possible);
}

void MdnsResponderService::StartListening() {
  if (bound_interfaces_.empty()) {
    mdns_responder_->Init();
    bound_interfaces_ = platform_->RegisterInterfaces({});
    for (auto& interface : bound_interfaces_) {
      mdns_responder_->RegisterInterface(interface.interface_info,
                                         interface.subnet, interface.socket);
    }
  }
  mdns::DomainName service_type;
  CHECK(mdns::DomainName::FromLabels(service_type_.begin(), service_type_.end(),
                                     &service_type));
  mdns_responder_->StartPtrQuery(service_type);
}

void MdnsResponderService::StopListening() {
  mdns::DomainName service_type;
  CHECK(mdns::DomainName::FromLabels(service_type_.begin(), service_type_.end(),
                                     &service_type));
  for (const auto& hostname : hostname_watchers_) {
    mdns_responder_->StopAQuery(hostname.first);
    mdns_responder_->StopAaaaQuery(hostname.first);
  }
  hostname_watchers_.clear();
  for (const auto& service : services_) {
    mdns_responder_->StopSrvQuery(service.first);
    mdns_responder_->StopTxtQuery(service.first);
  }
  services_.clear();
  mdns_responder_->StopPtrQuery(service_type);
  RemoveAllScreens();
}

void MdnsResponderService::StartService() {
  if (!bound_interfaces_.empty()) {
    std::vector<MdnsPlatformService::BoundInterface> deregistered_interfaces;
    for (auto it = bound_interfaces_.begin(); it != bound_interfaces_.end();) {
      if (std::find(interface_index_whitelist_.begin(),
                    interface_index_whitelist_.end(),
                    it->interface_info.index) ==
          interface_index_whitelist_.end()) {
        mdns_responder_->DeregisterInterface(it->socket);
        deregistered_interfaces.push_back(*it);
        it = bound_interfaces_.erase(it);
      } else {
        ++it;
      }
    }
    platform_->DeregisterInterfaces(deregistered_interfaces);
  } else {
    mdns_responder_->Init();
    bound_interfaces_ =
        platform_->RegisterInterfaces(interface_index_whitelist_);
    for (auto& interface : bound_interfaces_) {
      mdns_responder_->RegisterInterface(interface.interface_info,
                                         interface.subnet, interface.socket);
    }
  }
  mdns_responder_->SetHostLabel(service_hostname_);
  mdns::DomainName domain_name;
  CHECK(mdns::DomainName::FromLabels(&service_hostname_, &service_hostname_ + 1,
                                     &domain_name))
      << "bad hostname configured: " << service_hostname_;
  CHECK(domain_name.Append(mdns::DomainName::kLocalDomain));
  mdns_responder_->RegisterService(service_instance_name_, service_type_[0],
                                   service_type_[1], domain_name, service_port_,
                                   service_txt_lines_);
}

void MdnsResponderService::StopService() {
  mdns_responder_->DeregisterService(service_instance_name_, service_type_[0],
                                     service_type_[1]);
}

void MdnsResponderService::StopMdnsResponder() {
  mdns_responder_->Close();
  platform_->DeregisterInterfaces(bound_interfaces_);
  bound_interfaces_.clear();
  hostname_watchers_.clear();
  services_.clear();
  RemoveAllScreens();
}

void MdnsResponderService::PushScreenInfo(
    const mdns::DomainName& service_instance,
    const ServiceInstance& instance_info,
    const IPAddress& address) {
  std::string screen_id;
  screen_id.assign(
      reinterpret_cast<const char*>(service_instance.domain_name().data()),
      service_instance.domain_name().size());

  std::string friendly_name;
  for (const auto& line : instance_info.txt_info) {
    // TODO(btolsch): Placeholder until TXT data is spec'd.
    if (line.size() > 3 &&
        std::equal(std::begin("fn="), std::begin("fn=") + 3, line.begin())) {
      friendly_name = line.substr(3);
      break;
    }
  }

  auto entry = screen_info_.find(screen_id);
  if (entry == screen_info_.end()) {
    ScreenInfo screen_info{std::move(screen_id),
                           std::move(friendly_name),
                           instance_info.ptr_interface_index,
                           {address, instance_info.port}};
    listener_->OnScreenAdded(screen_info);
    screen_info_.emplace(screen_info.screen_id, std::move(screen_info));
  } else {
    auto& screen_info = entry->second;
    if (screen_info.Update(std::move(friendly_name),
                           instance_info.ptr_interface_index,
                           {address, instance_info.port})) {
      listener_->OnScreenChanged(screen_info);
    }
  }
}

void MdnsResponderService::MaybePushScreenInfo(
    const mdns::DomainName& service_instance,
    const ServiceInstance& instance_info) {
  if (!instance_info.ptr_interface_index || instance_info.txt_info.empty() ||
      instance_info.domain_name.IsEmpty() || instance_info.port == 0) {
    return;
  }
  auto entry = hostname_watchers_.find(instance_info.domain_name);
  if (entry == hostname_watchers_.end()) {
    return;
  }
  PushScreenInfo(service_instance, instance_info, entry->second.address);
}

void MdnsResponderService::MaybePushScreenInfo(
    const mdns::DomainName& domain_name,
    const IPAddress& address) {
  for (auto& entry : services_) {
    if (entry.second->domain_name == domain_name) {
      PushScreenInfo(entry.first, *entry.second, address);
    }
  }
}

void MdnsResponderService::RemoveScreenInfo(
    const mdns::DomainName& service_instance) {
  const auto screen_id = ScreenIdFromServiceInstance(service_instance);
  auto entry = screen_info_.find(screen_id);
  if (entry == screen_info_.end())
    return;
  listener_->OnScreenRemoved(entry->second);
  screen_info_.erase(entry);
}

void MdnsResponderService::RemoveScreenInfoByDomain(
    const mdns::DomainName& domain_name) {
  for (const auto& entry : services_) {
    if (entry.second->domain_name == domain_name) {
      RemoveScreenInfo(entry.first);
    }
  }
}

void MdnsResponderService::RemoveAllScreens() {
  bool had_screens = !screen_info_.empty();
  screen_info_.clear();
  if (had_screens) {
    listener_->OnAllScreensRemoved();
  }
}

bool MdnsResponderService::HandlePtrEvent(const mdns::PtrEvent& ptr_event) {
  bool events_possible = false;
  auto entry = services_.find(ptr_event.service_instance);
  switch (ptr_event.header.response_type) {
    case mdns::QueryEventHeader::Type::kAdded:
    case mdns::QueryEventHeader::Type::kAddedNoCache: {
      auto socket = ptr_event.header.socket;
      auto it = std::find_if(
          bound_interfaces_.begin(), bound_interfaces_.end(),
          [socket](const MdnsPlatformService::BoundInterface& interface) {
            return socket == interface.socket;
          });
      if (it != bound_interfaces_.end()) {
        int32_t interface_index = it->interface_info.index;
        mdns_responder_->StartSrvQuery(ptr_event.service_instance);
        mdns_responder_->StartTxtQuery(ptr_event.service_instance);
        events_possible = true;

        if (entry == services_.end()) {
          auto new_instance = MakeUnique<ServiceInstance>();
          new_instance->ptr_interface_index = interface_index;
          auto result = services_.emplace(std::move(ptr_event.service_instance),
                                          std::move(new_instance));
          entry = result.first;
        } else {
          entry->second->ptr_interface_index = interface_index;
        }
        MaybePushScreenInfo(ptr_event.service_instance, *entry->second);
      }
    } break;
    case mdns::QueryEventHeader::Type::kRemoved:
      if (entry != services_.end()) {
        // |port| == 0 signals that we also have no SRV record, and should
        // consider this service to be gone.
        if (entry->second->port == 0) {
          mdns_responder_->StopSrvQuery(ptr_event.service_instance);
          mdns_responder_->StopTxtQuery(ptr_event.service_instance);
          services_.erase(entry);
        } else {
          entry->second->ptr_interface_index = 0;
          RemoveScreenInfo(ptr_event.service_instance);
        }
      }
      break;
  }
  return events_possible;
}

bool MdnsResponderService::HandleSrvEvent(const mdns::SrvEvent& srv_event) {
  bool events_possible = false;
  auto entry = services_.find(srv_event.service_instance);
  switch (srv_event.header.response_type) {
    case mdns::QueryEventHeader::Type::kAdded:
    case mdns::QueryEventHeader::Type::kAddedNoCache: {
      mdns_responder_->StartAQuery(srv_event.domain_name);
      mdns_responder_->StartAaaaQuery(srv_event.domain_name);
      auto result =
          hostname_watchers_.emplace(srv_event.domain_name, HostnameWatchers{});
      auto hostname_entry = result.first;
      hostname_entry->second.services.push_back(entry->second.get());
      events_possible = true;
      if (entry == services_.end()) {
        auto result = services_.emplace(std::move(srv_event.service_instance),
                                        MakeUnique<ServiceInstance>());
        entry = result.first;
      }
      entry->second->domain_name = std::move(srv_event.domain_name);
      entry->second->port = srv_event.port;
      MaybePushScreenInfo(srv_event.service_instance, *entry->second);
    } break;
    case mdns::QueryEventHeader::Type::kRemoved: {
      auto hostname_entry = hostname_watchers_.find(entry->second->domain_name);
      hostname_entry->second.services.erase(
          std::remove_if(hostname_entry->second.services.begin(),
                         hostname_entry->second.services.end(),
                         [entry](ServiceInstance* instance) {
                           return instance == entry->second.get();
                         }));
      if (hostname_entry->second.services.empty()) {
        mdns_responder_->StopAQuery(hostname_entry->first);
        mdns_responder_->StopAaaaQuery(hostname_entry->first);
        hostname_watchers_.erase(hostname_entry);
      }
      // |ptr_interface_index| == 0 signals that we also have no PTR record,
      // and should consider this service to be gone.
      if (entry->second->ptr_interface_index == 0) {
        mdns_responder_->StopSrvQuery(srv_event.service_instance);
        mdns_responder_->StopTxtQuery(srv_event.service_instance);
        services_.erase(entry);
      } else {
        entry->second->domain_name = mdns::DomainName();
        entry->second->port = 0;
        RemoveScreenInfo(srv_event.service_instance);
      }
    } break;
  }
  return events_possible;
}

bool MdnsResponderService::HandleTxtEvent(const mdns::TxtEvent& txt_event) {
  bool events_possible = false;
  auto entry = services_.find(txt_event.service_instance);
  switch (txt_event.header.response_type) {
    case mdns::QueryEventHeader::Type::kAdded:
    case mdns::QueryEventHeader::Type::kAddedNoCache:
      if (entry == services_.end()) {
        auto result = services_.emplace(std::move(txt_event.service_instance),
                                        MakeUnique<ServiceInstance>());
        entry = result.first;
      }
      entry->second->txt_info = std::move(txt_event.txt_info);
      MaybePushScreenInfo(txt_event.service_instance, *entry->second);
      break;
    case mdns::QueryEventHeader::Type::kRemoved:
      entry->second->txt_info.clear();
      RemoveScreenInfo(txt_event.service_instance);
      break;
  }
  return events_possible;
}

bool MdnsResponderService::HandleAEvent(const mdns::AEvent& a_event) {
  bool events_possible = false;
  switch (a_event.header.response_type) {
    case mdns::QueryEventHeader::Type::kAdded:
    case mdns::QueryEventHeader::Type::kAddedNoCache: {
      auto& watchers = hostname_watchers_[a_event.domain_name];
      watchers.address = a_event.address;
      MaybePushScreenInfo(a_event.domain_name, watchers.address);
    } break;
    case mdns::QueryEventHeader::Type::kRemoved: {
      RemoveScreenInfoByDomain(a_event.domain_name);
      hostname_watchers_.erase(a_event.domain_name);
    } break;
  }
  return events_possible;
}

bool MdnsResponderService::HandleAaaaEvent(const mdns::AaaaEvent& aaaa_event) {
  bool events_possible = false;
  switch (aaaa_event.header.response_type) {
    case mdns::QueryEventHeader::Type::kAdded:
    case mdns::QueryEventHeader::Type::kAddedNoCache: {
      auto& watchers = hostname_watchers_[aaaa_event.domain_name];
      watchers.address = aaaa_event.address;
      MaybePushScreenInfo(aaaa_event.domain_name, watchers.address);
    } break;
    case mdns::QueryEventHeader::Type::kRemoved: {
      RemoveScreenInfoByDomain(aaaa_event.domain_name);
      hostname_watchers_.erase(aaaa_event.domain_name);
    } break;
  }
  return events_possible;
}

}  // namespace openscreen
