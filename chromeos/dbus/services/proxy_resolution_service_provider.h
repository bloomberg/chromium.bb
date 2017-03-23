// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SERVICES_PROXY_RESOLUTION_SERVICE_PROVIDER_H_
#define CHROMEOS_DBUS_SERVICES_PROXY_RESOLUTION_SERVICE_PROVIDER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"
#include "net/base/completion_callback.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}

namespace dbus {
class MethodCall;
}

namespace net {
class ProxyInfo;
class ProxyService;
class URLRequestContextGetter;
}

namespace chromeos {

// This class provides proxy resolution service for CrosDBusService.
// It processes proxy resolution requests for ChromeOS clients.
//
// The following method is exported:
//
// Interface: org.chromium.LibCrosServiceInterface (kLibCrosServiceInterface)
// Method: ResolveNetworkProxy (kResolveNetworkProxy)
// Parameters: string:source_url
//             string:signal_interface (optional)
//             string:signal_name (optional)
//
//   Resolves the proxy for |source_url|. If |signal_interface| and
//   |signal_name| are supplied, returns an empty reply immediately and
//   asynchronously emits a D-Bus signal to the requested destination.
//   Otherwise, returns proxy information an asynchronous response without
//   emitting a signal.
//
//   The signal (if requested) will contain three values:
//   - string:source_url - requested source URL.
//   - string:proxy_info - proxy info for the source URL in PAC format
//                         like "PROXY cache.example.com:12345"
//   - string:error_message - error message. Empty if successful.
//
//   The method call response (if requested) will contain just two values:
//   - string:proxy_info - proxy info for the source URL in PAC format
//                         like "PROXY cache.example.com:12345"
//   - string:error_message - error message. Empty if successful.
//
// This service can be manually tested using dbus-send:
//
//   % dbus-send --system --type=method_call --print-reply
//       --dest=org.chromium.LibCrosService
//       /org/chromium/LibCrosService
//       org.chromium.LibCrosServiceInterface.ResolveNetworkProxy
//       string:https://www.google.com/
//
class CHROMEOS_EXPORT ProxyResolutionServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  // Delegate interface providing additional resources to
  // ProxyResolutionServiceProvider.
  class CHROMEOS_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

    // Returns the request context used to perform proxy resolution.
    // Always called on UI thread.
    virtual scoped_refptr<net::URLRequestContextGetter> GetRequestContext() = 0;

    // Thin wrapper around net::ProxyService::ResolveProxy() to make testing
    // easier.
    virtual int ResolveProxy(net::ProxyService* proxy_service,
                             const GURL& url,
                             net::ProxyInfo* results,
                             const net::CompletionCallback& callback) = 0;
  };

  explicit ProxyResolutionServiceProvider(std::unique_ptr<Delegate> delegate);
  ~ProxyResolutionServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Data used for a single proxy resolution.
  struct Request;

  // Returns true if called on |origin_thread_|.
  bool OnOriginThread();

  // Called when ResolveProxy() is exported as a D-Bus method.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Callback invoked when Chrome OS clients send network proxy resolution
  // requests to the service. Called on UI thread.
  void ResolveProxy(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);

  // Helper method for ResolveProxy() that runs on network thread.
  void ResolveProxyOnNetworkThread(std::unique_ptr<Request> request);

  // Callback on network thread for when net::ProxyService::ResolveProxy()
  // completes, synchronously or asynchronously.
  void OnResolutionComplete(std::unique_ptr<Request> request, int result);

  // Called on UI thread from OnResolutionComplete() to pass the resolved proxy
  // information to the client over D-Bus.
  void NotifyProxyResolved(std::unique_ptr<Request> request);

  std::unique_ptr<Delegate> delegate_;
  scoped_refptr<dbus::ExportedObject> exported_object_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_thread_;
  base::WeakPtrFactory<ProxyResolutionServiceProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolutionServiceProvider);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SERVICES_PROXY_RESOLUTION_SERVICE_PROVIDER_H_
