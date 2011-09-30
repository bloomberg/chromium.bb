// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_CROS_DBUS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_CROS_DBUS_SERVICE_H_
#pragma once

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/platform_thread.h"

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
//
class ProxyResolutionService;

class CrosDBusService {
 public:
  // CrosDBusService consists of service providers that implement this
  // interface.
  //
  // ServiceProviderInterface is a ref counted object, to ensure that
  // |this| of the object is alive when callbacks referencing |this| are
  // called.
  class ServiceProviderInterface
      : public base::RefCountedThreadSafe<ServiceProviderInterface> {
   public:

    // Starts the service provider. |exported_object| is used to export
    // D-Bus methods.
    virtual void Start(
        scoped_refptr<dbus::ExportedObject> exported_object) = 0;

   protected:
    // This is protected, so we can define sub classes.
    virtual ~ServiceProviderInterface();

   private:
    friend class base::RefCountedThreadSafe<ServiceProviderInterface>;
  };

  virtual ~CrosDBusService();

  // Starts the D-Bus service.
  virtual void Start() = 0;

  // Creates the instance.
  static CrosDBusService* Create(dbus::Bus* bus);

 private:
  // Creates the instance for testing. Takes the ownership of
  // |proxy_resolution_service|.
  friend class CrosDBusServiceTest;
  static CrosDBusService* CreateForTesting(
      dbus::Bus* bus,
      ServiceProviderInterface* proxy_resolution_service);
};

}  // namespace

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_CROS_DBUS_SERVICE_H_
