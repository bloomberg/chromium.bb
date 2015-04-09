// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/mdns/mdns_api.h"

#include <vector>

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/api/mdns.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_messages.h"

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
  extensions::EventRouter* event_router = EventRouter::Get(context);
  DCHECK(event_router);
  event_router->RegisterObserver(this, mdns::OnServiceList::kEventName);
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
  if (dns_sd_registry_.get())
    dns_sd_registry_.get()->AddObserver(this);
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
  GetValidOnServiceListListeners(
      "" /* service_type_filter - blank = all services */,
      nullptr /* extension_ids */,
      &new_service_types);

  // Find all the added and removed service types since last update.
  std::set<std::string> added_service_types =
      base::STLSetDifference<std::set<std::string> >(
          new_service_types, service_types_);
  std::set<std::string> removed_service_types =
      base::STLSetDifference<std::set<std::string> >(
          service_types_, new_service_types);

  // Update the registry.
  DnsSdRegistry* registry = dns_sd_registry();
  for (const auto& srv : added_service_types) {
    registry->RegisterDnsSdListener(srv);
  }
  for (const auto& srv : removed_service_types) {
    registry->UnregisterDnsSdListener(srv);
  }
  service_types_ = new_service_types;
}

void MDnsAPI::OnDnsSdEvent(const std::string& service_type,
                           const DnsSdRegistry::DnsSdServiceList& services) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<linked_ptr<mdns::MDnsService> > args;
  for (DnsSdRegistry::DnsSdServiceList::const_iterator it = services.begin();
       it != services.end(); ++it) {
    if (static_cast<long>(args.size()) ==
        api::mdns::MAX_SERVICE_INSTANCES_PER_EVENT) {
      // TODO(reddaly): This is not the most meaningful way of notifying the
      // application that something bad happened.  It will go to the user's
      // console (which most users don't look at)and the developer will be none
      // the wiser.  Instead, changing the event to pass the number of
      // discovered instances would allow the caller to know when the list is
      // truncated and tell the user something meaningful in the extension/app.
      WriteToConsole(service_type,
                     content::CONSOLE_MESSAGE_LEVEL_WARNING,
                     base::StringPrintf(
                         "Truncating number of service instances in "
                         "onServiceList to maximum allowed: %d",
                         api::mdns::MAX_SERVICE_INSTANCES_PER_EVENT));
      break;
    }
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

  // TODO(justinlin): To avoid having listeners without filters getting all
  // events, modify API to have this event require filters.
  // TODO(reddaly): If event isn't on whitelist, ensure it does not get
  // broadcast to extensions.
  extensions::EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
}

void MDnsAPI::GetValidOnServiceListListeners(
    const std::string& service_type_filter,
    std::set<std::string>* extension_ids,
    std::set<std::string>* service_types) {
  for (const auto& listener :
         extensions::EventRouter::Get(browser_context_)->listeners()
         .GetEventListenersByName(mdns::OnServiceList::kEventName)) {
    base::DictionaryValue* filter = listener->filter();

    std::string service_type;
    filter->GetStringASCII(kEventFilterServiceTypeKey, &service_type);
    if (service_type.empty())
      continue;

    // Match service type when filter isn't ""
    if (!service_type_filter.empty() && service_type_filter != service_type)
      continue;

    const Extension* extension = ExtensionRegistry::Get(browser_context_)->
        enabled_extensions().GetByID(listener->extension_id());
    // Don't listen for services associated only with disabled extensions.
    if (!extension)
      continue;

    // Platform apps may query for all services; other types of extensions are
    // restricted to a whitelist.
    if (!extension->is_platform_app() &&
        !IsServiceTypeWhitelisted(service_type))
      continue;

    if (extension_ids)
      extension_ids->insert(listener->extension_id());
    if (service_types)
      service_types->insert(service_type);
  }
}

void MDnsAPI::WriteToConsole(const std::string& service_type,
                             content::ConsoleMessageLevel level,
                             const std::string& message) {
  // Get all the extensions with an onServiceList listener for a particular
  // service type.
  std::set<std::string> extension_ids;
  GetValidOnServiceListListeners(service_type,
                                 &extension_ids,
                                 nullptr /* service_types */);

  std::string logged_message(std::string("[chrome.mdns] ") + message);

  // Log to the consoles of the background pages for those extensions.
  for (const std::string& extension_id : extension_ids) {
    extensions::ExtensionHost* host =
        extensions::ProcessManager::Get(browser_context_)
        ->GetBackgroundHostForExtension(extension_id);
    if (!host)
      continue;
    content::RenderViewHost* rvh = host->render_view_host();
    if (!rvh)
      continue;
    rvh->Send(new ExtensionMsg_AddMessageToConsole(
        rvh->GetRoutingID(),
        level,
        logged_message));
  }
}

}  // namespace extensions
