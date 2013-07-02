// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/tools/service_discovery_sniffer/service_discovery_sniffer.h"

namespace local_discovery {

ServicePrinter::ServicePrinter(std::string service_name) : changed_(false) {
  service_resolver_ =
      ServiceDiscoveryClient::GetInstance()->CreateServiceResolver(
          service_name,
          base::Bind(&ServicePrinter::OnServiceResolved,
                     base::Unretained(this)));
}

ServicePrinter::~ServicePrinter() {
}

void ServicePrinter::Added() {
  changed_ = false;
  service_resolver_->StartResolving();
}

void ServicePrinter::Changed() {
  changed_ = true;
  service_resolver_->StartResolving();
}

void ServicePrinter::Removed() {
  printf("Service Removed: %s\n",
         service_resolver_->GetServiceDescription().instance_name().c_str());
}

void ServicePrinter::OnServiceResolved(ServiceResolver::RequestStatus status,
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

ServiceTypePrinter::ServiceTypePrinter(std::string service_type)  {
  watcher_ = ServiceDiscoveryClient::GetInstance()->CreateServiceWatcher(
      service_type, this);
}

bool ServiceTypePrinter::Start() {
  if (!watcher_->Start()) return false;
  watcher_->DiscoverNewServices(false);
  watcher_->ReadCachedServices();
  return true;
}

ServiceTypePrinter::~ServiceTypePrinter() {
}

void ServiceTypePrinter::OnServiceUpdated(ServiceWatcher::UpdateType update,
                                   const std::string& service_name) {
  if (update == ServiceWatcher::UPDATE_ADDED) {
    services_[service_name].reset(new ServicePrinter(service_name));
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
    local_discovery::ServiceTypePrinter print_changes(
        std::string(argv[1]) + "._tcp.local");

    if (!print_changes.Start())
      printf("Could not start the DNS-SD client\n");
    message_loop.Run();
  }
}
