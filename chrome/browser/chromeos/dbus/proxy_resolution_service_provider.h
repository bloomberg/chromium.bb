// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_PROXY_RESOLUTION_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_PROXY_RESOLUTION_SERVICE_PROVIDER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/chromeos/dbus/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace dbus {
class MethodCall;
class Response;
}

namespace chromeos {

class ProxyResolverInterface;

// This class provides proxy resolution service for CrosDBusService.
// It processes proxy resolution requests for ChromeOS clients.
//
// The following methods are exported.
//
// Interface: org.chromium.LibCrosServiceInterface (kLibCrosServiceInterface)
// Method: ResolveNetworkProxy (kResolveNetworkProxy)
// Parameters: string:source_url
//             string:signal_interface
//             string:signal_name
//
//   Resolves the proxy for |source_url|. Returns the result
//   as a D-Bus signal sent to |signal_interface| and |signal_name|.
//
//   The returned signal will contain the three values:
//   - string:source_url - requested source URL.
//   - string:proxy_info - proxy info for the source URL in PAC format
//                         like "PROXY cache.example.com:12345"
//   - string:error_message - error message. Empty if successful.
//
// This service can be manually tested using dbus-monitor and
// dbus-send. For instance, you can resolve proxy configuration for
// http://www.gmail.com/ as follows:
//
// 1. Open a terminal and run the following:
//
//   % dbus-monitor --system interface=org.chromium.TestInterface
//
// 2. Open another terminal and run the following:
//
//   % dbus-send --system --type=method_call
//       --dest=org.chromium.LibCrosService
//       /org/chromium/LibCrosService
//       org.chromium.LibCrosServiceInterface.ResolveNetworkProxy
//       string:http://www.gmail.com/
//       string:org.chromium.TestInterface
//       string:TestSignal
//
// 3. Go back to the original terminal and check the output which should
// look like:
//
// signal sender=:1.23 -> dest=(null destination) serial=12345
// path=/org/chromium/LibCrosService; interface=org.chromium.TestInterface;
// member=TestSignal
//   string "http://www.gmail.com/"
//   string "PROXY proxy.example.com:8080"
//   string ""
//

class ProxyResolutionServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  virtual ~ProxyResolutionServiceProvider();

  // CrosDBusService::ServiceProviderInterface override.
  virtual void Start(
      scoped_refptr<dbus::ExportedObject> exported_object) OVERRIDE;

  // Creates the instance.
  static ProxyResolutionServiceProvider* Create();

 private:
  explicit ProxyResolutionServiceProvider(ProxyResolverInterface *resovler);

  // Creates the instance for testing. Takes the ownership of |resovler|
  friend class ProxyResolutionServiceProviderTest;
  static ProxyResolutionServiceProvider* CreateForTesting(
      ProxyResolverInterface* resolver);

  // Called from ExportedObject, when ResolveProxyHandler() is exported as
  // a D-Bus method, or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Callback to be invoked when ChromeOS clients send network proxy
  // resolution requests to the service running in chrome executable.
  // Called on UI thread from dbus request.
  void ResolveProxyHandler(dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Calls ResolveProxyHandler() if weak_ptr is not NULL. Used to ensure a
  // safe shutdown.
  static void CallResolveProxyHandler(
      base::WeakPtr<ProxyResolutionServiceProvider> weak_ptr,
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread();

  scoped_refptr<dbus::ExportedObject> exported_object_;
  scoped_ptr<ProxyResolverInterface> resolver_;
  base::PlatformThreadId origin_thread_id_;
  base::WeakPtrFactory<ProxyResolutionServiceProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolutionServiceProvider);
};

// The interface is defined so we can mock out the proxy resolver
// implementation.
class ProxyResolverInterface {
 public:
  // Resolves the proxy for the given URL. Returns the result as a
  // signal sent to |signal_interface| and
  // |signal_name|. |exported_object| will be used to send the
  // signal. The signal contains the three string members:
  //
  // - source url: the requested source URL.
  // - proxy info: proxy info for the source URL in PAC format.
  // - error message: empty if the proxy resolution was successful.
  virtual void ResolveProxy(
      const std::string& source_url,
      const std::string& signal_interface,
      const std::string& signal_name,
      scoped_refptr<dbus::ExportedObject> exported_object) = 0;

  virtual ~ProxyResolverInterface();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_PROXY_RESOLUTION_SERVICE_PROVIDER_H_
