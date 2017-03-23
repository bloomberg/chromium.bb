// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_CHROME_PROXY_RESOLVER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_CHROME_PROXY_RESOLVER_DELEGATE_H_

#include "base/macros.h"
#include "chromeos/dbus/services/proxy_resolution_service_provider.h"

namespace chromeos {

// Chrome's implementation of ProxyResolverDelegate.
class ChromeProxyResolverDelegate : public ProxyResolverDelegate {
 public:
  ChromeProxyResolverDelegate();
  ~ChromeProxyResolverDelegate() override;

  // ProxyResolverDelegate override.
  scoped_refptr<net::URLRequestContextGetter> GetRequestContext() override;
  int ResolveProxy(net::ProxyService* proxy_service,
                   const GURL& url,
                   net::ProxyInfo* results,
                   const net::CompletionCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeProxyResolverDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_CHROME_PROXY_RESOLVER_DELEGATE_H_
