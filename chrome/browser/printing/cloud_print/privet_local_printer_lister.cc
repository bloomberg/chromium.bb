// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_local_printer_lister.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "chrome/browser/printing/cloud_print/privet_constants.h"
#include "chrome/browser/printing/cloud_print/privet_device_lister_impl.h"
#include "chrome/browser/printing/cloud_print/privet_http_asynchronous_factory.h"

namespace cloud_print {

struct PrivetLocalPrinterLister::DeviceContext {
  DeviceContext() : has_local_printing(false) {
  }

  scoped_ptr<PrivetHTTPResolution> privet_resolution;
  scoped_ptr<PrivetHTTPClient> privet_client;
  scoped_ptr<PrivetJSONOperation> info_operation;
  DeviceDescription description;

  bool has_local_printing;
};

PrivetLocalPrinterLister::PrivetLocalPrinterLister(
    local_discovery::ServiceDiscoveryClient* service_discovery_client,
    net::URLRequestContextGetter* request_context,
    Delegate* delegate) : delegate_(delegate) {
  privet_lister_.reset(
      new PrivetDeviceListerImpl(service_discovery_client, this));
  privet_http_factory_ = PrivetHTTPAsynchronousFactory::CreateInstance(
      request_context);
}

PrivetLocalPrinterLister::~PrivetLocalPrinterLister() {
}

void PrivetLocalPrinterLister::Start() {
  privet_lister_->Start();
  privet_lister_->DiscoverNewDevices(false);
}

void PrivetLocalPrinterLister::Stop() {
  privet_lister_.reset();
}

void PrivetLocalPrinterLister::DeviceChanged(
    bool added,
    const std::string& name,
    const DeviceDescription& description) {
  if (description.type != kPrivetTypePrinter)
    return;

  DeviceContextMap::iterator i = device_contexts_.find(name);

  if (i != device_contexts_.end()) {
    i->second->description = description;
    delegate_->LocalPrinterChanged(added, name, i->second->has_local_printing,
                                   description);
  } else {
    linked_ptr<DeviceContext> context(new DeviceContext);
    context->has_local_printing = false;
    context->description = description;
    context->privet_resolution = privet_http_factory_->CreatePrivetHTTP(name);
    device_contexts_[name] = context;
    context->privet_resolution->Start(
        description.address,
        base::Bind(&PrivetLocalPrinterLister::OnPrivetResolved,
                   base::Unretained(this), name));
  }
}

void PrivetLocalPrinterLister::DeviceCacheFlushed() {
  device_contexts_.clear();
  delegate_->LocalPrinterCacheFlushed();
}

void PrivetLocalPrinterLister::OnPrivetResolved(
    const std::string& name,
    scoped_ptr<PrivetHTTPClient> http_client) {
  if (!http_client) {
    // Remove device if we can't resolve it.
    device_contexts_.erase(name);
    return;
  }
  DeviceContextMap::iterator i = device_contexts_.find(http_client->GetName());
  DCHECK(i != device_contexts_.end());

  i->second->info_operation = http_client->CreateInfoOperation(
      base::Bind(&PrivetLocalPrinterLister::OnPrivetInfoDone,
                 base::Unretained(this),
                 i->second.get(),
                 http_client->GetName()));
  i->second->privet_client = std::move(http_client);
  i->second->info_operation->Start();
}

void PrivetLocalPrinterLister::OnPrivetInfoDone(
    DeviceContext* context,
    const std::string& name,
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

  context->has_local_printing = has_local_printing;
  delegate_->LocalPrinterChanged(true, name, has_local_printing,
                                 context->description);
}

void PrivetLocalPrinterLister::DeviceRemoved(const std::string& device_name) {
  DeviceContextMap::iterator i = device_contexts_.find(device_name);
  if (i != device_contexts_.end()) {
    device_contexts_.erase(i);
    delegate_->LocalPrinterRemoved(device_name);
  }
}

const DeviceDescription* PrivetLocalPrinterLister::GetDeviceDescription(
    const std::string& name) {
  DeviceContextMap::iterator i = device_contexts_.find(name);
  if (i == device_contexts_.end()) return NULL;
  return &i->second->description;
}

}  // namespace cloud_print
