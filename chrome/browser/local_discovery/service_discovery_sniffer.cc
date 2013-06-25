// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/local_discovery/service_discovery_sniffer.h"

namespace local_discovery {

Service::Service(std::string service_name) : changed_(false) {
  service_resolver_ =
      ServiceDiscoveryClient::GetInstance()->CreateServiceResolver(
          service_name,
          base::Bind(&Service::OnServiceResolved, base::Unretained(this)));
}

Service::~Service() {
}

void Service::Added() {
  changed_ = false;
  service_resolver_->StartResolving();
}

void Service::Changed() {
  changed_ = true;
  service_resolver_->StartResolving();
}

void Service::Removed() {
  printf("Service Removed: %s\n",
         service_resolver_->GetServiceDescription().instance_name().c_str());
}

void Service::OnServiceResolved(ServiceResolver::RequestStatus status,
                                const ServiceDescription& service) {
  if (changed_) {
    printf("Service Updated: %s\n", service.instance_name().c_str());
  } else {
    printf("Service Added: %s\n", service.instance_name().c_str());
  }

  printf("\tAddress: %s:%d\n", service.address.host().c_str(),
         service.address.port());
  printf("\tMetadata: \n");
  for (std::vector<std::string>::const_iterator i = service.metadata.begin();
       i != service.metadata.end(); i++) {
    printf("\t\t%s\n", i->c_str());
  }

  if (service.ip_address != net::IPAddressNumber()) {
    printf("\tIP Address: %s\n", net::IPAddressToString(
        service.ip_address).c_str());
  }
}

ServiceType::ServiceType(std::string service_type) : watcher_(
    ServiceDiscoveryClient::GetInstance()->CreateServiceWatcher(
        service_type, this))  {
  watcher_->Start();
  watcher_->DiscoverNewServices(false);
  watcher_->ReadCachedServices();
}

ServiceType::~ServiceType() {
}

void ServiceType::OnServiceUpdated(ServiceWatcher::UpdateType update,
                                   const std::string& service_name) {
  if (update == ServiceWatcher::UPDATE_ADDED) {
    services_[service_name].reset(new Service(service_name));
    services_[service_name]->Added();
  } else if (update == ServiceWatcher::UPDATE_CHANGED) {
    services_[service_name]->Changed();
  } else if (update == ServiceWatcher::UPDATE_REMOVED) {
    services_[service_name]->Removed();
    services_.erase(service_name);
  }
}

}  // namespace local_discovery

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;

  if (argc != 2) {
    printf("Please provide exactly 1 argument.\n");
    return 1;
  }

  { // To guarantee/make explicit the ordering constraint.
    local_discovery::ServiceType print_changes(
        std::string(argv[1]) + "._tcp.local");

    message_loop.Run();
  }
}
