// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_V8_H_
#define NET_PROXY_PROXY_RESOLVER_V8_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/proxy/proxy_resolver.h"

namespace net {

// Implementation of ProxyResolver that uses V8 to evaluate PAC scripts.
class NET_EXPORT_PRIVATE ProxyResolverV8 : public ProxyResolver {
 public:
  // Interface for the javascript bindings.
  class NET_EXPORT_PRIVATE JSBindings {
   public:
    enum ResolveDnsOperation {
      DNS_RESOLVE,
      DNS_RESOLVE_EX,
      MY_IP_ADDRESS,
      MY_IP_ADDRESS_EX,
    };

    JSBindings() {}

    // Handler for "dnsResolve()", "dnsResolveEx()", "myIpAddress()",
    // "myIpAddressEx()". Returns true on success and fills |*output| with the
    // result. If |*terminate| is set to true, then the script execution will
    // be aborted. Note that termination may not happen right away.
    virtual bool ResolveDns(const std::string& host,
                            ResolveDnsOperation op,
                            std::string* output,
                            bool* terminate) = 0;

    // Handler for "alert(message)"
    virtual void Alert(const base::string16& message) = 0;

    // Handler for when an error is encountered. |line_number| may be -1
    // if a line number is not applicable to this error.
    virtual void OnError(int line_number, const base::string16& error) = 0;

   protected:
    virtual ~JSBindings() {}
  };

  // Constructs a ProxyResolverV8.
  ProxyResolverV8();

  ~ProxyResolverV8() override;

  JSBindings* js_bindings() const { return js_bindings_; }
  void set_js_bindings(JSBindings* js_bindings) { js_bindings_ = js_bindings; }

  // ProxyResolver implementation:
  int GetProxyForURL(const GURL& url,
                     ProxyInfo* results,
                     const net::CompletionCallback& /*callback*/,
                     RequestHandle* /*request*/,
                     const BoundNetLog& net_log) override;
  void CancelRequest(RequestHandle request) override;
  LoadState GetLoadState(RequestHandle request) const override;
  void CancelSetPacScript() override;
  int SetPacScript(const scoped_refptr<ProxyResolverScriptData>& script_data,
                   const net::CompletionCallback& /*callback*/) override;

  // Get total/ued heap memory usage of all v8 instances used by the proxy
  // resolver.
  static size_t GetTotalHeapSize();
  static size_t GetUsedHeapSize();

 private:
  // Context holds the Javascript state for the most recently loaded PAC
  // script. It corresponds with the data from the last call to
  // SetPacScript().
  class Context;

  scoped_ptr<Context> context_;

  JSBindings* js_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverV8);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_V8_H_
