// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/chrome_proxy_resolution_service_provider_delegate.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {

ChromeProxyResolutionServiceProviderDelegate::
    ChromeProxyResolutionServiceProviderDelegate() {}

ChromeProxyResolutionServiceProviderDelegate::
    ~ChromeProxyResolutionServiceProviderDelegate() {}

scoped_refptr<net::URLRequestContextGetter>
ChromeProxyResolutionServiceProviderDelegate::GetRequestContext() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  return profile->GetRequestContext();
}

int ChromeProxyResolutionServiceProviderDelegate::ResolveProxy(
    net::ProxyService* proxy_service,
    const GURL& url,
    net::ProxyInfo* results,
    const net::CompletionCallback& callback) {
  DCHECK(proxy_service);
  DCHECK(results);
  return proxy_service->ResolveProxy(url, std::string(), results, callback,
                                     nullptr, nullptr, net::NetLogWithSource());
}

}  // namespace chromeos
