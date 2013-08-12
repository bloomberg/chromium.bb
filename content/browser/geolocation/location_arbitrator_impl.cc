// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/location_arbitrator_impl.h"

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/browser/geolocation/network_location_provider.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "url/gurl.h"

namespace content {
namespace {

const char* kDefaultNetworkProviderUrl =
    "https://www.googleapis.com/geolocation/v1/geolocate";
}  // namespace

// To avoid oscillations, set this to twice the expected update interval of a
// a GPS-type location provider (in case it misses a beat) plus a little.
const int64 GeolocationArbitratorImpl::kFixStaleTimeoutMilliseconds =
    11 * base::Time::kMillisecondsPerSecond;

GeolocationArbitratorImpl::GeolocationArbitratorImpl(
    const LocationUpdateCallback& callback)
    : callback_(callback),
      provider_callback_(
          base::Bind(&GeolocationArbitratorImpl::LocationUpdateAvailable,
                     base::Unretained(this))),
      position_provider_(NULL),
      is_permission_granted_(false),
      is_running_(false) {
}

GeolocationArbitratorImpl::~GeolocationArbitratorImpl() {
}

GURL GeolocationArbitratorImpl::DefaultNetworkProviderURL() {
  return GURL(kDefaultNetworkProviderUrl);
}

void GeolocationArbitratorImpl::OnPermissionGranted() {
  is_permission_granted_ = true;
  for (ScopedVector<LocationProvider>::iterator i = providers_.begin();
      i != providers_.end(); ++i) {
    (*i)->OnPermissionGranted();
  }
}

void GeolocationArbitratorImpl::StartProviders(bool use_high_accuracy) {
  // Stash options as OnAccessTokenStoresLoaded has not yet been called.
  is_running_ = true;
  use_high_accuracy_ = use_high_accuracy;
  if (providers_.empty()) {
    DCHECK(DefaultNetworkProviderURL().is_valid());
    GetAccessTokenStore()->LoadAccessTokens(
        base::Bind(&GeolocationArbitratorImpl::OnAccessTokenStoresLoaded,
                   base::Unretained(this)));
  } else {
    DoStartProviders();
  }
}

void GeolocationArbitratorImpl::DoStartProviders() {
  for (ScopedVector<LocationProvider>::iterator i = providers_.begin();
       i != providers_.end(); ++i) {
    (*i)->StartProvider(use_high_accuracy_);
  }
}

void GeolocationArbitratorImpl::StopProviders() {
  // Reset the reference location state (provider+position)
  // so that future starts use fresh locations from
  // the newly constructed providers.
  position_provider_ = NULL;
  position_ = Geoposition();

  providers_.clear();
  is_running_ = false;
}

void GeolocationArbitratorImpl::OnAccessTokenStoresLoaded(
    AccessTokenStore::AccessTokenSet access_token_set,
    net::URLRequestContextGetter* context_getter) {
  if (!is_running_ || !providers_.empty()) {
    // A second StartProviders() call may have arrived before the first
    // completed.
    return;
  }
  // If there are no access tokens, boot strap it with the default server URL.
  if (access_token_set.empty())
    access_token_set[DefaultNetworkProviderURL()];
  for (AccessTokenStore::AccessTokenSet::iterator i =
           access_token_set.begin();
      i != access_token_set.end(); ++i) {
    RegisterProvider(
        NewNetworkLocationProvider(
            GetAccessTokenStore(), context_getter,
            i->first, i->second));
  }

  LocationProvider* provider =
      GetContentClient()->browser()->OverrideSystemLocationProvider();
  if (!provider)
    provider = NewSystemLocationProvider();
  RegisterProvider(provider);
  DoStartProviders();
}

void GeolocationArbitratorImpl::RegisterProvider(
    LocationProvider* provider) {
  if (!provider)
    return;
  provider->SetUpdateCallback(provider_callback_);
  if (is_permission_granted_)
    provider->OnPermissionGranted();
  providers_.push_back(provider);
}

void GeolocationArbitratorImpl::LocationUpdateAvailable(
    const LocationProvider* provider,
    const Geoposition& new_position) {
  DCHECK(new_position.Validate() ||
         new_position.error_code != Geoposition::ERROR_CODE_NONE);
  if (!IsNewPositionBetter(position_, new_position,
                           provider == position_provider_))
    return;
  position_provider_ = provider;
  position_ = new_position;
  callback_.Run(position_);
}

AccessTokenStore* GeolocationArbitratorImpl::NewAccessTokenStore() {
  return GetContentClient()->browser()->CreateAccessTokenStore();
}

AccessTokenStore* GeolocationArbitratorImpl::GetAccessTokenStore() {
  if (!access_token_store_.get())
    access_token_store_ = NewAccessTokenStore();
  return access_token_store_.get();
}

LocationProvider* GeolocationArbitratorImpl::NewNetworkLocationProvider(
    AccessTokenStore* access_token_store,
    net::URLRequestContextGetter* context,
    const GURL& url,
    const string16& access_token) {
#if defined(OS_ANDROID)
  // Android uses its own SystemLocationProvider.
  return NULL;
#else
  return new NetworkLocationProvider(access_token_store, context, url,
                                     access_token);
#endif
}

LocationProvider* GeolocationArbitratorImpl::NewSystemLocationProvider() {
#if defined(OS_WIN)
  return NULL;
#else
  return content::NewSystemLocationProvider();
#endif
}

base::Time GeolocationArbitratorImpl::GetTimeNow() const {
  return base::Time::Now();
}

bool GeolocationArbitratorImpl::IsNewPositionBetter(
    const Geoposition& old_position, const Geoposition& new_position,
    bool from_same_provider) const {
  // Updates location_info if it's better than what we currently have,
  // or if it's a newer update from the same provider.
  if (!old_position.Validate()) {
    // Older location wasn't locked.
    return true;
  }
  if (new_position.Validate()) {
    // New location is locked, let's check if it's any better.
    if (old_position.accuracy >= new_position.accuracy) {
      // Accuracy is better.
      return true;
    } else if (from_same_provider) {
      // Same provider, fresher location.
      return true;
    } else if ((GetTimeNow() - old_position.timestamp).InMilliseconds() >
               kFixStaleTimeoutMilliseconds) {
      // Existing fix is stale.
      return true;
    }
  }
  return false;
}

bool GeolocationArbitratorImpl::HasPermissionBeenGranted() const {
  return is_permission_granted_;
}

}  // namespace content
