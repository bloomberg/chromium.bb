// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/chrome_proxy_resolver_delegate.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {

ChromeProxyResolverDelegate::ChromeProxyResolverDelegate() {}

ChromeProxyResolverDelegate::~ChromeProxyResolverDelegate() {}

scoped_refptr<net::URLRequestContextGetter>
ChromeProxyResolverDelegate::GetRequestContext() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  return profile->GetRequestContext();
}

}  // namespace chromeos
