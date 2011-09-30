// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/cros_dbus_service.h"

#include "base/stl_util.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/chromeos/dbus/proxy_resolution_service_provider.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "content/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// The CrosDBusService implementation used in production, and unit tests.
class CrosDBusServiceImpl : public CrosDBusService {
 public:
  explicit CrosDBusServiceImpl(dbus::Bus* bus)
      : service_started_(false),
        origin_thread_id_(base::PlatformThread::CurrentId()),
        bus_(bus) {
  }

  virtual ~CrosDBusServiceImpl() {
  }

  // CrosDBusService override.
  virtual void Start() {
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

    VLOG(1) << "CrosDBusServiceImpl started.";
  }

  // Registers a service provider. This must be done before Start().
  // |provider| will be owned by CrosDBusService.
  void RegisterServiceProvider(ServiceProviderInterface* provider) {
    service_providers_.push_back(provider);
  }

 private:
  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread() {
    return base::PlatformThread::CurrentId() == origin_thread_id_;
  }

  bool service_started_;
  base::PlatformThreadId origin_thread_id_;
  dbus::Bus* bus_;
  scoped_refptr<dbus::ExportedObject> exported_object_;

  // Service providers that form CrosDBusService.
  std::vector<scoped_refptr<ServiceProviderInterface> > service_providers_;
};

// The stub CrosDBusService implementation used on Linux desktop,
// which does nothing as of now.
class CrosDBusServiceStubImpl : public CrosDBusService {
 public:
  CrosDBusServiceStubImpl() {
  }

  virtual ~CrosDBusServiceStubImpl() {
  }

  // CrosDBusService override.
  virtual void Start() {
  }
};

// static
CrosDBusService* CrosDBusService::Get(dbus::Bus* bus) {
  if (system::runtime_environment::IsRunningOnChromeOS()) {
    CrosDBusServiceImpl* service = new CrosDBusServiceImpl(bus);
    service->RegisterServiceProvider(ProxyResolutionServiceProvider::Get());
    return service;
  } else {
    return new CrosDBusServiceStubImpl;
  }
}

// static
CrosDBusService* CrosDBusService::GetForTesting(
    dbus::Bus* bus,
    ServiceProviderInterface* proxy_resolution_service) {
  CrosDBusServiceImpl* service =  new CrosDBusServiceImpl(bus);
  service->RegisterServiceProvider(proxy_resolution_service);
  return service;
}

CrosDBusService::~CrosDBusService() {
}

CrosDBusService::ServiceProviderInterface::~ServiceProviderInterface() {
}

}  // namespace chromeos
