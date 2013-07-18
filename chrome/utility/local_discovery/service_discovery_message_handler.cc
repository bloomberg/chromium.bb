// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/local_discovery/service_discovery_message_handler.h"

#include "chrome/common/local_discovery/local_discovery_messages.h"
#include "chrome/utility/local_discovery/service_discovery_client_impl.h"
#include "content/public/utility/utility_thread.h"

namespace local_discovery {

ServiceDiscoveryMessageHandler::ServiceDiscoveryMessageHandler() {
}

ServiceDiscoveryMessageHandler::~ServiceDiscoveryMessageHandler() {
}

void ServiceDiscoveryMessageHandler::Initialize() {
  if (!service_discovery_client_) {
    mdns_client_ = net::MDnsClient::CreateDefault();
    mdns_client_->StartListening();
    service_discovery_client_.reset(
        new local_discovery::ServiceDiscoveryClientImpl(mdns_client_.get()));
  }
}

bool ServiceDiscoveryMessageHandler::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceDiscoveryMessageHandler, message)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_StartWatcher, OnStartWatcher)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_DiscoverServices, OnDiscoverServices)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_DestroyWatcher, OnDestroyWatcher)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_ResolveService, OnResolveService)
    IPC_MESSAGE_HANDLER(LocalDiscoveryMsg_DestroyResolver, OnDestroyResolver)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ServiceDiscoveryMessageHandler::OnStartWatcher(
    uint64 id,
    const std::string& service_type) {
  Initialize();
  DCHECK(!ContainsKey(service_watchers_, id));
  scoped_ptr<ServiceWatcher> watcher(
      service_discovery_client_->CreateServiceWatcher(
          service_type,
          base::Bind(&ServiceDiscoveryMessageHandler::OnServiceUpdated,
                     base::Unretained(this), id)));
  watcher->Start();
  service_watchers_[id].reset(watcher.release());
}

void ServiceDiscoveryMessageHandler::OnDiscoverServices(uint64 id,
                                                       bool force_update) {
  DCHECK(ContainsKey(service_watchers_, id));
  service_watchers_[id]->DiscoverNewServices(force_update);
}

void ServiceDiscoveryMessageHandler::OnDestroyWatcher(uint64 id) {
  DCHECK(ContainsKey(service_watchers_, id));
  service_watchers_.erase(id);
}

void ServiceDiscoveryMessageHandler::OnResolveService(
    uint64 id,
    const std::string& service_name) {
  Initialize();
  DCHECK(!ContainsKey(service_resolvers_, id));
  scoped_ptr<ServiceResolver> resolver(
      service_discovery_client_->CreateServiceResolver(
          service_name,
          base::Bind(&ServiceDiscoveryMessageHandler::OnServiceResolved,
                     base::Unretained(this), id)));
  resolver->StartResolving();
  service_resolvers_[id].reset(resolver.release());
}

void ServiceDiscoveryMessageHandler::OnDestroyResolver(uint64 id) {
  DCHECK(ContainsKey(service_resolvers_, id));
  service_resolvers_.erase(id);
}

void ServiceDiscoveryMessageHandler::OnServiceUpdated(
    uint64 id,
    ServiceWatcher::UpdateType update,
    const std::string& name) {
  content::UtilityThread::Get()->Send(
      new LocalDiscoveryHostMsg_WatcherCallback(id, update, name));
}

void ServiceDiscoveryMessageHandler::OnServiceResolved(
    uint64 id,
    ServiceResolver::RequestStatus status,
    const ServiceDescription& description) {
  content::UtilityThread::Get()->Send(
      new LocalDiscoveryHostMsg_ResolverCallback(id, status, description));
}

}  // namespace local_discovery
