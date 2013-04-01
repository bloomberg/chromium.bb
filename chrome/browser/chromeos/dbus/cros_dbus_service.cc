// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/cros_dbus_service.h"

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/stl_util.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/chromeos/dbus/display_power_service_provider.h"
#include "chrome/browser/chromeos/dbus/liveness_service_provider.h"
#include "chrome/browser/chromeos/dbus/printer_service_provider.h"
#include "chrome/browser/chromeos/dbus/proxy_resolution_service_provider.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

CrosDBusService* g_cros_dbus_service = NULL;

}  // namespace

// The CrosDBusService implementation used in production, and unit tests.
class CrosDBusServiceImpl : public CrosDBusService {
 public:
  explicit CrosDBusServiceImpl(dbus::Bus* bus)
      : service_started_(false),
        origin_thread_id_(base::PlatformThread::CurrentId()),
        bus_(bus) {
  }

  virtual ~CrosDBusServiceImpl() {
    STLDeleteElements(&service_providers_);
  }

  // Starts the D-Bus service.
  void Start() {
    // Make sure we're running on the origin thread (i.e. the UI thread in
    // production).
    DCHECK(OnOriginThread());

    // Return if the service has been already started.
    if (service_started_)
      return;

    bus_->RequestOwnership(kLibCrosServiceName,
                           base::Bind(&CrosDBusServiceImpl::OnOwnership,
                                      base::Unretained(this)));

    exported_object_ = bus_->GetExportedObject(
        dbus::ObjectPath(kLibCrosServicePath));

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

  // Called when an ownership request is completed.
  void OnOwnership(const std::string& service_name,
                   bool success) {
    LOG_IF(FATAL, !success) << "Failed to own: " << service_name;
  }

  bool service_started_;
  base::PlatformThreadId origin_thread_id_;
  dbus::Bus* bus_;
  scoped_refptr<dbus::ExportedObject> exported_object_;

  // Service providers that form CrosDBusService.
  std::vector<ServiceProviderInterface*> service_providers_;
};

// The stub CrosDBusService implementation used on Linux desktop,
// which does nothing as of now.
class CrosDBusServiceStubImpl : public CrosDBusService {
 public:
  CrosDBusServiceStubImpl() {
  }

  virtual ~CrosDBusServiceStubImpl() {
  }
};

// static
void CrosDBusService::Initialize() {
  if (g_cros_dbus_service) {
    LOG(WARNING) << "CrosDBusService was already initialized";
    return;
  }
  dbus::Bus* bus = DBusThreadManager::Get()->GetSystemBus();
  if (base::chromeos::IsRunningOnChromeOS() && bus) {
    CrosDBusServiceImpl* service = new CrosDBusServiceImpl(bus);
    service->RegisterServiceProvider(ProxyResolutionServiceProvider::Create());
    service->RegisterServiceProvider(new DisplayPowerServiceProvider);
    service->RegisterServiceProvider(new LivenessServiceProvider);
    service->RegisterServiceProvider(new PrinterServiceProvider);
    g_cros_dbus_service = service;
    service->Start();
  } else {
    g_cros_dbus_service = new CrosDBusServiceStubImpl;
  }
  VLOG(1) << "CrosDBusService initialized";
}

// static
void CrosDBusService::InitializeForTesting(
    dbus::Bus* bus,
    ServiceProviderInterface* proxy_resolution_service) {
  if (g_cros_dbus_service) {
    LOG(WARNING) << "CrosDBusService was already initialized";
    return;
  }
  CrosDBusServiceImpl* service =  new CrosDBusServiceImpl(bus);
  service->RegisterServiceProvider(proxy_resolution_service);
  service->Start();
  g_cros_dbus_service = service;
  VLOG(1) << "CrosDBusService initialized";
}

// static
void CrosDBusService::Shutdown() {
  delete g_cros_dbus_service;
  g_cros_dbus_service = NULL;
  VLOG(1) << "CrosDBusService Shutdown completed";
}

CrosDBusService::~CrosDBusService() {
}

CrosDBusService::ServiceProviderInterface::~ServiceProviderInterface() {
}

}  // namespace chromeos
