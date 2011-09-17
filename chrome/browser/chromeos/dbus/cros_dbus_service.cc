// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/cros_dbus_service.h"

#include "base/stl_util.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/chromeos/dbus/proxy_resolution_service_provider.h"
#include "content/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

CrosDBusService::CrosDBusService(dbus::Bus* bus)
    : service_started_(false),
      origin_thread_id_(base::PlatformThread::CurrentId()),
      bus_(bus) {
}

CrosDBusService::~CrosDBusService() {
}

void CrosDBusService::Start() {
  // Make sure we're running on the origin thread (i.e. the UI thread in
  // production).
  DCHECK(OnOriginThread());

  // Return if the service has been already started.
  if (service_started_)
    return;

  exported_object_ = bus_->GetExportedObject(
      kLibCrosServiceName,
      kLibCrosServicePath);

  for (size_t i = 0; i < service_providers_.size(); ++i)
    service_providers_[i]->Start(exported_object_);

  service_started_ = true;

  VLOG(1) << "CrosDBusService started.";
}

CrosDBusService* CrosDBusService::Get(dbus::Bus* bus) {
  CrosDBusService* service = new CrosDBusService(bus);
  service->RegisterServiceProvider(ProxyResolutionServiceProvider::Get());
  return service;
}

CrosDBusService* CrosDBusService::GetForTesting(
    dbus::Bus* bus,
    ServiceProviderInterface* proxy_resolution_service) {
  CrosDBusService* service =  new CrosDBusService(bus);
  service->RegisterServiceProvider(proxy_resolution_service);
  return service;
}

void CrosDBusService::RegisterServiceProvider(
    ServiceProviderInterface* provider) {
  service_providers_.push_back(provider);
}

bool CrosDBusService::OnOriginThread() {
  return base::PlatformThread::CurrentId() == origin_thread_id_;
}

}  // namespace chromeos
