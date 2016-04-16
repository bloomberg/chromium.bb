// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_V8_TRACING_H_
#define NET_PROXY_PROXY_RESOLVER_V8_TRACING_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_resolver_factory.h"

namespace net {

class HostResolver;

// ProxyResolverV8Tracing is a non-blocking proxy resolver.
class NET_EXPORT ProxyResolverV8Tracing {
 public:
  // Bindings is an interface used by ProxyResolverV8Tracing to delegate
  // per-request functionality. Each instance will be destroyed on the origin
  // thread of the ProxyResolverV8Tracing when the request completes or is
  // cancelled. All methods will be invoked from the origin thread.
  class Bindings {
   public:
    Bindings() {}
    virtual ~Bindings() {}

    // Invoked in response to an alert() call by the PAC script.
    virtual void Alert(const base::string16& message) = 0;

    // Invoked in response to an error in the PAC script.
    virtual void OnError(int line_number, const base::string16& message) = 0;

    // Returns a HostResolver to use for DNS resolution.
    virtual HostResolver* GetHostResolver() = 0;

    // Returns a BoundNetLog to be passed to the HostResolver returned by
    // GetHostResolver().
    virtual BoundNetLog GetBoundNetLog() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Bindings);
  };

  virtual ~ProxyResolverV8Tracing() {}

  // Gets a list of proxy servers to use for |url|. This request always
  // runs asynchronously and notifies the result by running |callback|. If the
  // result code is OK then the request was successful and |results| contains
  // the proxy resolution information. If |request| is non-null, |*request| is
  // written to, and can be passed to CancelRequest().
  virtual void GetProxyForURL(const GURL& url,
                              ProxyInfo* results,
                              const CompletionCallback& callback,
                              ProxyResolver::RequestHandle* request,
                              std::unique_ptr<Bindings> bindings) = 0;

  // Cancels |request|.
  virtual void CancelRequest(ProxyResolver::RequestHandle request) = 0;

  // Gets the LoadState for |request|.
  virtual LoadState GetLoadState(
      ProxyResolver::RequestHandle request) const = 0;
};

// A factory for ProxyResolverV8Tracing instances. The default implementation,
// returned by Create(), creates ProxyResolverV8Tracing instances that execute
// ProxyResolverV8 on a single helper thread, and do some magic to avoid
// blocking in DNS. For more details see the design document:
// https://docs.google.com/a/google.com/document/d/16Ij5OcVnR3s0MH4Z5XkhI9VTPoMJdaBn9rKreAmGOdE/edit?pli=1
class NET_EXPORT ProxyResolverV8TracingFactory {
 public:
  ProxyResolverV8TracingFactory() {}
  virtual ~ProxyResolverV8TracingFactory() = default;

  virtual void CreateProxyResolverV8Tracing(
      const scoped_refptr<ProxyResolverScriptData>& pac_script,
      std::unique_ptr<ProxyResolverV8Tracing::Bindings> bindings,
      std::unique_ptr<ProxyResolverV8Tracing>* resolver,
      const CompletionCallback& callback,
      std::unique_ptr<ProxyResolverFactory::Request>* request) = 0;

  static std::unique_ptr<ProxyResolverV8TracingFactory> Create();

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyResolverV8TracingFactory);
};

// This enum is used by an UMA histogram, so the values shouldn't be reordered
// or renumbered.
//
// TODO(eroman): Remove when done gathering data for crbug.com/593759
enum class PacResultForStrippedUrl {
  // Did NOT measure the impact of running FindProxyForURL() with a modified
  // URL path, because the original URL could not complete using the
  // non-blocking DNS mode.
  SKIPPED_FALLBACK_BLOCKING_DNS = 0,

  // The result of running FindProxyForURL() with a modified URL path appears
  // to be indistinguishable. (Although there may have been sideffects to the
  // script state that won't manifest until later invocations).
  SUCCESS = 1,

  // Calling FindProxyForURL() with a modified URL path returned the same proxy
  // list, but had measurable sideffects in calls to alert().
  SUCCESS_DIFFERENT_ALERTS = 2,

  // Calling FindProxyForURL() with a modified URL path returned the same proxy
  // list, but invoked a different sequence of DNS resolutions. This would
  // require a rather unusual script to trigger.
  SUCCESS_DIFFERENT_NUM_DNS = 3,

  // Calling FindProxyForURL() with a modified URL path resulted in a different
  // set of DNS dependencies.
  FAIL_ABANDONED = 4,

  // Calling FindProxyForURL() with a modified URL path caused a different
  // execution flow. Whereas with the original URL it succeeded with
  // non-blocking DNS, this attempt requires fallback to blocking DNS (and was
  // not attempted).
  FAIL_FALLBACK_BLOCKING_DNS = 5,

  // Calling FindProxyForURL() with a modified URL path caused a script error.
  FAIL_ERROR = 6,

  // Calling FindProxyForURL() with a modified URL path returned a different
  // proxy list.
  FAIL_DIFFERENT_PROXY_LIST = 7,

  MAX_VALUE,
};

// TODO(eroman): Remove when done gathering data for crbug.com/593759
//
// This histogram name is exported only for the sake of unit-tests.
extern NET_EXPORT_PRIVATE const char kHistogramPacResultForStrippedUrl[];

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_V8_TRACING_H_
