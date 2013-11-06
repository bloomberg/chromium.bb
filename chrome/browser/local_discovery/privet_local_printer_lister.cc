// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_local_printer_lister.h"

#include <string>

#include "chrome/browser/local_discovery/privet_constants.h"
#include "chrome/browser/local_discovery/privet_device_lister_impl.h"
#include "chrome/browser/local_discovery/privet_http_asynchronous_factory.h"

namespace local_discovery {

struct PrivetLocalPrinterLister::DeviceContext {
 public:
  DeviceContext() {
  }

  ~DeviceContext() {
  }

  scoped_ptr<PrivetHTTPResolution> privet_resolution;
  scoped_ptr<PrivetHTTPClient> privet_client;
  scoped_ptr<PrivetInfoOperation> info_operation;
  DeviceDescription description;

  bool has_local_printing;
};

PrivetLocalPrinterLister::PrivetLocalPrinterLister(
    ServiceDiscoveryClient* service_discovery_client,
    net::URLRequestContextGetter* request_context,
    Delegate* delegate) : delegate_(delegate) {
  privet_lister_.reset(new PrivetDeviceListerImpl(service_discovery_client,
                                                  this,
                                                  kPrivetSubtypePrinter));
  privet_http_factory_ = PrivetHTTPAsynchronousFactory::CreateInstance(
      service_discovery_client,
      request_context);
}

PrivetLocalPrinterLister::~PrivetLocalPrinterLister() {
}

void PrivetLocalPrinterLister::Start() {
  privet_lister_->Start();
  privet_lister_->DiscoverNewDevices(false);
}

void PrivetLocalPrinterLister::DeviceChanged(
    bool added,
    const std::string& name,
    const DeviceDescription& description) {
  DeviceContextMap::iterator i = device_contexts_.find(name);

  if (i != device_contexts_.end()) {
    if (i->second->has_local_printing) {
      // This line helps with the edge case of a device description changing
      // during the /privet/info call.
      i->second->description = description;
      delegate_->LocalPrinterChanged(added, name, description);
    }
  } else {
    linked_ptr<DeviceContext> context(new DeviceContext);
    context->has_local_printing = false;
    context->description = description;
    context->privet_resolution = privet_http_factory_->CreatePrivetHTTP(
        name,
        description.address,
        base::Bind(&PrivetLocalPrinterLister::OnPrivetResolved,
                   base::Unretained(this)));

    device_contexts_[name] = context;
    context->privet_resolution->Start();
  }
}

void PrivetLocalPrinterLister::DeviceCacheFlushed() {
  device_contexts_.clear();
  delegate_->LocalPrinterCacheFlushed();
}

void PrivetLocalPrinterLister::OnPrivetResolved(
    scoped_ptr<PrivetHTTPClient> http_client) {
  DeviceContextMap::iterator i = device_contexts_.find(http_client->GetName());
  DCHECK(i != device_contexts_.end());

  i->second->info_operation = http_client->CreateInfoOperation(this);
  i->second->privet_client = http_client.Pass();
  i->second->info_operation->Start();
}

void PrivetLocalPrinterLister::OnPrivetInfoDone(
    PrivetInfoOperation* operation,
    int http_code,
    const base::DictionaryValue* json_value) {
  bool has_local_printing = false;
  const base::ListValue* api_list = NULL;
  if (json_value && json_value->GetList(kPrivetInfoKeyAPIList, &api_list)) {
    for (size_t i = 0; i < api_list->GetSize(); i++) {
      std::string api;
      api_list->GetString(i, &api);
      if (api == kPrivetSubmitdocPath) {
        has_local_printing = true;
      }
    }
  }

  std::string name = operation->GetHTTPClient()->GetName();
  DeviceContextMap::iterator i = device_contexts_.find(name);
  DCHECK(i != device_contexts_.end());
  i->second->has_local_printing = has_local_printing;
  if (has_local_printing) {
    delegate_->LocalPrinterChanged(true, name, i->second->description);
  }
}

void PrivetLocalPrinterLister::DeviceRemoved(const std::string& device_name) {
  DeviceContextMap::iterator i = device_contexts_.find(device_name);
  if (i != device_contexts_.end()) {
    bool has_local_printing = i->second->has_local_printing;
    device_contexts_.erase(i);
    if (has_local_printing) {
      delegate_->LocalPrinterRemoved(device_name);
    }
  }
}

}  // namespace local_discovery
