// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/mdns/mdns_api.h"

#include <vector>

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/api/mdns.h"

namespace extensions {

namespace mdns = api::mdns;

namespace {

// Whitelisted mDNS service types.
const char kCastServiceType[] = "_googlecast._tcp.local";
const char kPrivetServiceType[] = "_privet._tcp.local";
const char kTestServiceType[] = "_testing._tcp.local";

bool IsServiceTypeWhitelisted(const std::string& service_type) {
  return service_type == kCastServiceType ||
         service_type == kPrivetServiceType ||
         service_type == kTestServiceType;
}

}  // namespace

MDnsAPI::MDnsAPI(content::BrowserContext* context) : browser_context_(context) {
  DCHECK(browser_context_);
  EventRouter::Get(context)
      ->RegisterObserver(this, mdns::OnServiceList::kEventName);
}

MDnsAPI::~MDnsAPI() {
  if (dns_sd_registry_.get()) {
    dns_sd_registry_->RemoveObserver(this);
  }
}

// static
MDnsAPI* MDnsAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<MDnsAPI>::Get(context);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<MDnsAPI> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<MDnsAPI>* MDnsAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void MDnsAPI::SetDnsSdRegistryForTesting(
    scoped_ptr<DnsSdRegistry> dns_sd_registry) {
  dns_sd_registry_ = dns_sd_registry.Pass();
}

DnsSdRegistry* MDnsAPI::dns_sd_registry() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!dns_sd_registry_.get()) {
    dns_sd_registry_.reset(new extensions::DnsSdRegistry());
    dns_sd_registry_->AddObserver(this);
  }
  return dns_sd_registry_.get();
}

void MDnsAPI::OnListenerAdded(const EventListenerInfo& details) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateMDnsListeners(details);
}

void MDnsAPI::OnListenerRemoved(const EventListenerInfo& details) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateMDnsListeners(details);
}

void MDnsAPI::UpdateMDnsListeners(const EventListenerInfo& details) {
  std::set<std::string> new_service_types;

  // Check all listeners for service type filers.
  const EventListenerMap::ListenerList& listeners =
      extensions::EventRouter::Get(browser_context_)
          ->listeners()
          .GetEventListenersByName(details.event_name);
  for (EventListenerMap::ListenerList::const_iterator it = listeners.begin();
       it != listeners.end(); ++it) {
    base::DictionaryValue* filter = ((*it)->filter());

    std::string filter_value;
    filter->GetStringASCII(kEventFilterServiceTypeKey, &filter_value);
    if (filter_value.empty())
      continue;
    new_service_types.insert(filter_value);
  }

  // Find all the added and removed service types since last update.
  std::set<std::string> added_service_types =
      base::STLSetDifference<std::set<std::string> >(
          new_service_types, service_types_);
  std::set<std::string> removed_service_types =
      base::STLSetDifference<std::set<std::string> >(
          service_types_, new_service_types);

  // Update the registry.
  DnsSdRegistry* registry = dns_sd_registry();
  for (std::set<std::string>::iterator it = added_service_types.begin();
       it != added_service_types.end(); ++it) {
    if (IsServiceTypeWhitelisted(*it))
      registry->RegisterDnsSdListener(*it);
  }
  for (std::set<std::string>::iterator it = removed_service_types.begin();
       it != removed_service_types.end(); ++it) {
    if (IsServiceTypeWhitelisted(*it))
      registry->UnregisterDnsSdListener(*it);
  }

  service_types_ = new_service_types;
}

void MDnsAPI::OnDnsSdEvent(const std::string& service_type,
                           const DnsSdRegistry::DnsSdServiceList& services) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<linked_ptr<mdns::MDnsService> > args;
  for (DnsSdRegistry::DnsSdServiceList::const_iterator it = services.begin();
       it != services.end(); ++it) {
    linked_ptr<mdns::MDnsService> mdns_service =
        make_linked_ptr(new mdns::MDnsService);
    mdns_service->service_name = (*it).service_name;
    mdns_service->service_host_port = (*it).service_host_port;
    mdns_service->ip_address = (*it).ip_address;
    mdns_service->service_data = (*it).service_data;
    args.push_back(mdns_service);
  }

  scoped_ptr<base::ListValue> results = mdns::OnServiceList::Create(args);
  scoped_ptr<Event> event(
      new Event(mdns::OnServiceList::kEventName, results.Pass()));
  event->restrict_to_browser_context = browser_context_;
  event->filter_info.SetServiceType(service_type);

  VLOG(1) << "Broadcasting OnServiceList event: " << event.get();

  // TODO(justinlin): To avoid having listeners without filters getting all
  // events, modify API to have this event require filters.
  extensions::EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
}

}  // namespace extensions
