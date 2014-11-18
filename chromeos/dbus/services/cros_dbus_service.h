// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SERVICES_CROS_DBUS_SERVICE_H_
#define CHROMEOS_DBUS_SERVICES_CROS_DBUS_SERVICE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/platform_thread.h"
#include "chromeos/chromeos_export.h"

namespace dbus {
class Bus;
class ExportedObject;
}

namespace chromeos {

// CrosDBusService is used to run a D-Bus service inside Chrome for Chrome
// OS. The service will be registered as follows:
//
// Service name: org.chromium.LibCrosService (kLibCrosServiceName)
// Object path: chromium/LibCrosService (kLibCrosServicePath)
//
// For historical reasons, the rather irrelevant name "LibCrosService" is
// used in the D-Bus constants such as the service name.
//
// CrosDBusService exports D-Bus methods through service provider classes
// that implement CrosDBusService::ServiceProviderInterface.

class CHROMEOS_EXPORT CrosDBusService {
 public:
  // CrosDBusService consists of service providers that implement this
  // interface.
  class ServiceProviderInterface {
   public:
    // Starts the service provider. |exported_object| is used to export
    // D-Bus methods.
    virtual void Start(
        scoped_refptr<dbus::ExportedObject> exported_object) = 0;

    virtual ~ServiceProviderInterface();
  };

  // Initializes the global instance.
  static void Initialize(
      ScopedVector<ServiceProviderInterface> service_providers);
  // Destroys the global instance.
  static void Shutdown();

 protected:
  virtual ~CrosDBusService();

 private:
  friend class CrosDBusServiceTest;

  // Initializes the global instance for testing.
  static void InitializeForTesting(
      dbus::Bus* bus,
      ScopedVector<ServiceProviderInterface> service_providers);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SERVICES_CROS_DBUS_SERVICE_H_
