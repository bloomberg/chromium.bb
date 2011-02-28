// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/location_arbitrator.h"

#include <map>

#include "chrome/browser/profiles/profile.h"
#include "content/browser/geolocation/arbitrator_dependency_factory.h"

namespace {

const char* kDefaultNetworkProviderUrl = "https://www.google.com/loc/json";
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
      context_getter_(dependency_factory->GetContextGetter()),
      get_time_now_(dependency_factory->GetTimeFunction()),
      observer_(observer),
      position_provider_(NULL) {
  DCHECK(GURL(kDefaultNetworkProviderUrl).is_valid());
  access_token_store_->LoadAccessTokens(
      &request_consumer_,
      NewCallback(this,
                  &GeolocationArbitrator::OnAccessTokenStoresLoaded));
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

void GeolocationArbitrator::OnPermissionGranted(
    const GURL& requesting_frame) {
  most_recent_authorized_frame_ = requesting_frame;
  for (ScopedVector<LocationProviderBase>::iterator i = providers_.begin();
      i != providers_.end(); ++i) {
    (*i)->OnPermissionGranted(requesting_frame);
  }
}

void GeolocationArbitrator::StartProviders(
    const GeolocationObserverOptions& options) {
  // Stash options incase OnAccessTokenStoresLoaded has not yet been called
  // (in which case |providers_| will be empty).
  current_provider_options_ = options;
  StartProviders();
}

void GeolocationArbitrator::StartProviders() {
  for (ScopedVector<LocationProviderBase>::iterator i = providers_.begin();
       i != providers_.end(); ++i) {
    (*i)->StartProvider(current_provider_options_.use_high_accuracy);
  }
}

void GeolocationArbitrator::StopProviders() {
  for (ScopedVector<LocationProviderBase>::iterator i = providers_.begin();
       i != providers_.end(); ++i) {
    (*i)->StopProvider();
  }
}

void GeolocationArbitrator::OnAccessTokenStoresLoaded(
    AccessTokenStore::AccessTokenSet access_token_set) {
  DCHECK(providers_.empty())
      << "OnAccessTokenStoresLoaded : has existing location "
      << "provider. Race condition caused repeat load of tokens?";
  // If there are no access tokens, boot strap it with the default server URL.
  if (access_token_set.empty())
    access_token_set[GURL(kDefaultNetworkProviderUrl)];
  for (AccessTokenStore::AccessTokenSet::iterator i =
           access_token_set.begin();
      i != access_token_set.end(); ++i) {
    RegisterProvider(
        dependency_factory_->NewNetworkLocationProvider(
            access_token_store_.get(), context_getter_.get(),
            i->first, i->second));
  }
  RegisterProvider(dependency_factory_->NewSystemLocationProvider());
  StartProviders();
}

void GeolocationArbitrator::RegisterProvider(
    LocationProviderBase* provider) {
  if (!provider)
    return;
  provider->RegisterListener(this);
  if (most_recent_authorized_frame_.is_valid())
    provider->OnPermissionGranted(most_recent_authorized_frame_);
  providers_->push_back(provider);
}

void GeolocationArbitrator::LocationUpdateAvailable(
    LocationProviderBase* provider) {
  DCHECK(provider);
  Geoposition new_position;
  provider->GetPosition(&new_position);
  DCHECK(new_position.IsInitialized());
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
  if (!old_position.IsValidFix()) {
    // Older location wasn't locked.
    return true;
  }
  if (new_position.IsValidFix()) {
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
  return most_recent_authorized_frame_.is_valid();
}

void GeolocationArbitrator::SetDependencyFactoryForTest(
    GeolocationArbitratorDependencyFactory* dependency_factory) {
  g_dependency_factory_for_test = dependency_factory;
}
