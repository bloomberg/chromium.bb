// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/location_arbitrator.h"

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/browser/geolocation/arbitrator_dependency_factory.h"
#include "content/public/browser/access_token_store.h"
#include "googleurl/src/gurl.h"

using content::AccessTokenStore;
using content::Geoposition;

namespace {

const char* kDefaultNetworkProviderUrl =
    "https://www.googleapis.com/geolocation/v1/geolocate";
GeolocationArbitratorDependencyFactory* g_dependency_factory_for_test = NULL;

}  // namespace

// To avoid oscillations, set this to twice the expected update interval of a
// a GPS-type location provider (in case it misses a beat) plus a little.
const int64 GeolocationArbitrator::kFixStaleTimeoutMilliseconds =
    11 * base::Time::kMillisecondsPerSecond;

GeolocationArbitrator::GeolocationArbitrator(
    GeolocationArbitratorDependencyFactory* dependency_factory,
    GeolocationObserver* observer)
    : dependency_factory_(dependency_factory),
      access_token_store_(dependency_factory->NewAccessTokenStore()),
      get_time_now_(dependency_factory->GetTimeFunction()),
      observer_(observer),
      position_provider_(NULL),
      is_permission_granted_(false) {

}

GeolocationArbitrator::~GeolocationArbitrator() {
}

GeolocationArbitrator* GeolocationArbitrator::Create(
    GeolocationObserver* observer) {
  GeolocationArbitratorDependencyFactory* dependency_factory =
      g_dependency_factory_for_test ?
      g_dependency_factory_for_test :
      new DefaultGeolocationArbitratorDependencyFactory;
  GeolocationArbitrator* arbitrator =
      new GeolocationArbitrator(dependency_factory, observer);
  g_dependency_factory_for_test = NULL;
  return arbitrator;
}

GURL GeolocationArbitrator::DefaultNetworkProviderURL() {
  return GURL(kDefaultNetworkProviderUrl);
}

void GeolocationArbitrator::OnPermissionGranted() {
  is_permission_granted_ = true;
  for (ScopedVector<LocationProviderBase>::iterator i = providers_.begin();
      i != providers_.end(); ++i) {
    (*i)->OnPermissionGranted();
  }
}

void GeolocationArbitrator::StartProviders(
    const GeolocationObserverOptions& options) {
  // Stash options as OnAccessTokenStoresLoaded has not yet been called.
  current_provider_options_ = options;
  if (providers_.empty()) {
    DCHECK(DefaultNetworkProviderURL().is_valid());
    access_token_store_->LoadAccessTokens(
        base::Bind(&GeolocationArbitrator::OnAccessTokenStoresLoaded,
                   base::Unretained(this)));
  } else {
    DoStartProviders();
  }
}

void GeolocationArbitrator::DoStartProviders() {
  for (ScopedVector<LocationProviderBase>::iterator i = providers_.begin();
       i != providers_.end(); ++i) {
    (*i)->StartProvider(current_provider_options_.use_high_accuracy);
  }
}

void GeolocationArbitrator::StopProviders() {
  providers_.clear();
}

void GeolocationArbitrator::OnAccessTokenStoresLoaded(
    AccessTokenStore::AccessTokenSet access_token_set,
    net::URLRequestContextGetter* context_getter) {
  if (!providers_.empty()) {
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
        dependency_factory_->NewNetworkLocationProvider(
            access_token_store_.get(), context_getter,
            i->first, i->second));
  }
  RegisterProvider(dependency_factory_->NewSystemLocationProvider());
  DoStartProviders();
}

void GeolocationArbitrator::RegisterProvider(
    LocationProviderBase* provider) {
  if (!provider)
    return;
  provider->RegisterListener(this);
  if (is_permission_granted_)
    provider->OnPermissionGranted();
  providers_.push_back(provider);
}

void GeolocationArbitrator::LocationUpdateAvailable(
    LocationProviderBase* provider) {
  DCHECK(provider);
  Geoposition new_position;
  provider->GetPosition(&new_position);
  DCHECK(new_position.Validate() ||
         new_position.error_code != content::Geoposition::ERROR_CODE_NONE);
  if (!IsNewPositionBetter(position_, new_position,
                           provider == position_provider_))
    return;
  position_provider_ = provider;
  position_ = new_position;
  observer_->OnLocationUpdate(position_);
}

bool GeolocationArbitrator::IsNewPositionBetter(
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
    } else if ((get_time_now_() - old_position.timestamp).InMilliseconds() >
               kFixStaleTimeoutMilliseconds) {
      // Existing fix is stale.
      return true;
    }
  }
  return false;
}

bool GeolocationArbitrator::HasPermissionBeenGranted() const {
  return is_permission_granted_;
}

void GeolocationArbitrator::SetDependencyFactoryForTest(
    GeolocationArbitratorDependencyFactory* dependency_factory) {
  g_dependency_factory_for_test = dependency_factory;
}
