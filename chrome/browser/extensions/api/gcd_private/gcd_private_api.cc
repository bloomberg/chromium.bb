// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/gcd_private/gcd_private_api.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/local_discovery/cloud_device_list.h"
#include "chrome/browser/local_discovery/cloud_print_printer_list.h"
#include "chrome/browser/local_discovery/gcd_constants.h"
#include "chrome/browser/local_discovery/privet_device_lister_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"

namespace extensions {

namespace gcd_private = api::gcd_private;

namespace {

const int kNumRequestsNeeded = 2;

const char kIDPrefixCloudPrinter[] = "cloudprint:";
const char kIDPrefixGcd[] = "gcd:";
const char kIDPrefixMdns[] = "mdns:";

scoped_ptr<Event> MakeDeviceStateChangedEvent(
    const gcd_private::GCDDevice& device) {
  scoped_ptr<base::ListValue> params =
      gcd_private::OnDeviceStateChanged::Create(device);
  scoped_ptr<Event> event(
      new Event(gcd_private::OnDeviceStateChanged::kEventName, params.Pass()));
  return event.Pass();
}

scoped_ptr<Event> MakeDeviceRemovedEvent(const std::string& device) {
  scoped_ptr<base::ListValue> params =
      gcd_private::OnDeviceRemoved::Create(device);
  scoped_ptr<Event> event(
      new Event(gcd_private::OnDeviceRemoved::kEventName, params.Pass()));
  return event.Pass();
}

GcdPrivateAPI::GCDApiFlowFactoryForTests* g_gcd_api_flow_factory = NULL;

base::LazyInstance<BrowserContextKeyedAPIFactory<GcdPrivateAPI> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

scoped_ptr<local_discovery::GCDApiFlow> MakeGCDApiFlow(Profile* profile) {
  if (g_gcd_api_flow_factory) {
    return g_gcd_api_flow_factory->CreateGCDApiFlow();
  }

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  if (!token_service)
    return scoped_ptr<local_discovery::GCDApiFlow>();
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetInstance()->GetForProfile(profile);
  if (!signin_manager)
    return scoped_ptr<local_discovery::GCDApiFlow>();
  return local_discovery::GCDApiFlow::Create(
      profile->GetRequestContext(),
      token_service,
      signin_manager->GetAuthenticatedAccountId());
}

}  // namespace

GcdPrivateAPI::GcdPrivateAPI(content::BrowserContext* context)
    : num_device_listeners_(0), browser_context_(context) {
  DCHECK(browser_context_);
  if (EventRouter::Get(context)) {
    EventRouter::Get(context)
        ->RegisterObserver(this, gcd_private::OnDeviceStateChanged::kEventName);
    EventRouter::Get(context)
        ->RegisterObserver(this, gcd_private::OnDeviceRemoved::kEventName);
  }
}

GcdPrivateAPI::~GcdPrivateAPI() {
  if (EventRouter::Get(browser_context_)) {
    EventRouter::Get(browser_context_)->UnregisterObserver(this);
  }
}

// static
BrowserContextKeyedAPIFactory<GcdPrivateAPI>*
GcdPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void GcdPrivateAPI::OnListenerAdded(const EventListenerInfo& details) {
  if (details.event_name == gcd_private::OnDeviceStateChanged::kEventName ||
      details.event_name == gcd_private::OnDeviceRemoved::kEventName) {
    num_device_listeners_++;

    if (num_device_listeners_ == 1) {
      service_discovery_client_ =
          local_discovery::ServiceDiscoverySharedClient::GetInstance();
      privet_device_lister_.reset(new local_discovery::PrivetDeviceListerImpl(
          service_discovery_client_.get(), this));
      privet_device_lister_->Start();
    }

    for (GCDDeviceMap::iterator i = known_devices_.begin();
         i != known_devices_.end();
         i++) {
      EventRouter::Get(browser_context_)->DispatchEventToExtension(
          details.extension_id, MakeDeviceStateChangedEvent(*i->second));
    }
  }
}

void GcdPrivateAPI::OnListenerRemoved(const EventListenerInfo& details) {
  if (details.event_name == gcd_private::OnDeviceStateChanged::kEventName ||
      details.event_name == gcd_private::OnDeviceRemoved::kEventName) {
    num_device_listeners_--;

    if (num_device_listeners_ == 0) {
      privet_device_lister_.reset();
      service_discovery_client_ = NULL;
    }
  }
}

void GcdPrivateAPI::DeviceChanged(
    bool added,
    const std::string& name,
    const local_discovery::DeviceDescription& description) {
  linked_ptr<gcd_private::GCDDevice> device(new gcd_private::GCDDevice);
  device->setup_type = gcd_private::SETUP_TYPE_MDNS;
  device->device_id = kIDPrefixMdns + name;
  device->device_type = description.type;
  device->device_name = description.name;
  device->device_description = description.description;
  if (!description.id.empty())
    device->cloud_id.reset(new std::string(description.id));

  known_devices_[device->device_id] = device;

  EventRouter::Get(browser_context_)
      ->BroadcastEvent(MakeDeviceStateChangedEvent(*device));
}

void GcdPrivateAPI::DeviceRemoved(const std::string& name) {
  GCDDeviceMap::iterator found = known_devices_.find(kIDPrefixMdns + name);
  linked_ptr<gcd_private::GCDDevice> device = found->second;
  known_devices_.erase(found);

  EventRouter::Get(browser_context_)
      ->BroadcastEvent(MakeDeviceRemovedEvent(device->device_id));
}

void GcdPrivateAPI::DeviceCacheFlushed() {
  for (GCDDeviceMap::iterator i = known_devices_.begin();
       i != known_devices_.end();
       i++) {
    EventRouter::Get(browser_context_)
        ->BroadcastEvent(MakeDeviceRemovedEvent(i->second->device_id));
  }

  known_devices_.clear();
}

bool GcdPrivateAPI::QueryForDevices() {
  if (!privet_device_lister_)
    return false;

  privet_device_lister_->DiscoverNewDevices(true);

  return true;
}

// static
void GcdPrivateAPI::SetGCDApiFlowFactoryForTests(
    GCDApiFlowFactoryForTests* factory) {
  g_gcd_api_flow_factory = factory;
}

GcdPrivateGetCloudDeviceListFunction::GcdPrivateGetCloudDeviceListFunction() {
}

GcdPrivateGetCloudDeviceListFunction::~GcdPrivateGetCloudDeviceListFunction() {
}

bool GcdPrivateGetCloudDeviceListFunction::RunAsync() {
  requests_succeeded_ = 0;
  requests_failed_ = 0;

  printer_list_ = MakeGCDApiFlow(GetProfile());
  device_list_ = MakeGCDApiFlow(GetProfile());

  if (!printer_list_ || !device_list_)
    return false;

  // Balanced in CheckListingDone()
  AddRef();

  printer_list_->Start(make_scoped_ptr<local_discovery::GCDApiFlow::Request>(
      new local_discovery::CloudPrintPrinterList(this)));
  device_list_->Start(make_scoped_ptr<local_discovery::GCDApiFlow::Request>(
      new local_discovery::CloudDeviceList(this)));

  return true;
}

void GcdPrivateGetCloudDeviceListFunction::OnDeviceListReady(
    const DeviceList& devices) {
  requests_succeeded_++;

  devices_.insert(devices_.end(), devices.begin(), devices.end());

  CheckListingDone();
}

void GcdPrivateGetCloudDeviceListFunction::OnDeviceListUnavailable() {
  requests_failed_++;

  CheckListingDone();
}

void GcdPrivateGetCloudDeviceListFunction::CheckListingDone() {
  if (requests_failed_ + requests_succeeded_ != kNumRequestsNeeded)
    return;

  if (requests_succeeded_ == 0) {
    SendResponse(false);
    return;
  }

  std::vector<linked_ptr<gcd_private::GCDDevice> > devices;

  for (DeviceList::iterator i = devices_.begin(); i != devices_.end(); i++) {
    linked_ptr<gcd_private::GCDDevice> device(new gcd_private::GCDDevice);
    device->setup_type = gcd_private::SETUP_TYPE_CLOUD;
    if (i->type == local_discovery::kGCDTypePrinter) {
      device->device_id = kIDPrefixCloudPrinter + i->id;
    } else {
      device->device_id = kIDPrefixGcd + i->id;
    }

    device->cloud_id.reset(new std::string(i->id));
    device->device_type = i->type;
    device->device_name = i->display_name;
    device->device_description = i->description;

    devices.push_back(device);
  }

  results_ = gcd_private::GetCloudDeviceList::Results::Create(devices);

  SendResponse(true);
  Release();
}

GcdPrivateQueryForNewLocalDevicesFunction::
    GcdPrivateQueryForNewLocalDevicesFunction() {
}

GcdPrivateQueryForNewLocalDevicesFunction::
    ~GcdPrivateQueryForNewLocalDevicesFunction() {
}

bool GcdPrivateQueryForNewLocalDevicesFunction::RunSync() {
  GcdPrivateAPI* gcd_api =
      BrowserContextKeyedAPIFactory<GcdPrivateAPI>::Get(GetProfile());

  if (!gcd_api)
    return false;

  if (!gcd_api->QueryForDevices()) {
    error_ =
        "You must first subscribe to onDeviceStateChanged or onDeviceRemoved "
        "notifications";
    return false;
  }

  return true;
}

GcdPrivatePrefetchWifiPasswordFunction::
    GcdPrivatePrefetchWifiPasswordFunction() {
}

GcdPrivatePrefetchWifiPasswordFunction::
    ~GcdPrivatePrefetchWifiPasswordFunction() {
}

bool GcdPrivatePrefetchWifiPasswordFunction::RunAsync() {
  return false;
}

GcdPrivateEstablishSessionFunction::GcdPrivateEstablishSessionFunction() {
}

GcdPrivateEstablishSessionFunction::~GcdPrivateEstablishSessionFunction() {
}

bool GcdPrivateEstablishSessionFunction::RunAsync() {
  return false;
}

GcdPrivateConfirmCodeFunction::GcdPrivateConfirmCodeFunction() {
}

GcdPrivateConfirmCodeFunction::~GcdPrivateConfirmCodeFunction() {
}

bool GcdPrivateConfirmCodeFunction::RunAsync() {
  return false;
}

GcdPrivateSendMessageFunction::GcdPrivateSendMessageFunction() {
}

GcdPrivateSendMessageFunction::~GcdPrivateSendMessageFunction() {
}

bool GcdPrivateSendMessageFunction::RunAsync() {
  return false;
}

GcdPrivateTerminateSessionFunction::GcdPrivateTerminateSessionFunction() {
}

GcdPrivateTerminateSessionFunction::~GcdPrivateTerminateSessionFunction() {
}

bool GcdPrivateTerminateSessionFunction::RunAsync() {
  return false;
}

GcdPrivateGetCommandDefinitionsFunction::
    GcdPrivateGetCommandDefinitionsFunction() {
}

GcdPrivateGetCommandDefinitionsFunction::
    ~GcdPrivateGetCommandDefinitionsFunction() {
}

bool GcdPrivateGetCommandDefinitionsFunction::RunAsync() {
  return false;
}

GcdPrivateInsertCommandFunction::GcdPrivateInsertCommandFunction() {
}

GcdPrivateInsertCommandFunction::~GcdPrivateInsertCommandFunction() {
}

bool GcdPrivateInsertCommandFunction::RunAsync() {
  return false;
}

GcdPrivateGetCommandFunction::GcdPrivateGetCommandFunction() {
}

GcdPrivateGetCommandFunction::~GcdPrivateGetCommandFunction() {
}

bool GcdPrivateGetCommandFunction::RunAsync() {
  return false;
}

GcdPrivateCancelCommandFunction::GcdPrivateCancelCommandFunction() {
}

GcdPrivateCancelCommandFunction::~GcdPrivateCancelCommandFunction() {
}

bool GcdPrivateCancelCommandFunction::RunAsync() {
  return false;
}

GcdPrivateGetCommandsListFunction::GcdPrivateGetCommandsListFunction() {
}

GcdPrivateGetCommandsListFunction::~GcdPrivateGetCommandsListFunction() {
}

bool GcdPrivateGetCommandsListFunction::RunAsync() {
  return false;
}

}  // namespace extensions
