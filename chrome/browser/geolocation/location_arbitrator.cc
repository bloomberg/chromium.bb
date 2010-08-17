// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/location_arbitrator.h"

#include <map>

#include "base/logging.h"
#include "base/non_thread_safe.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/scoped_vector.h"
#include "chrome/browser/geolocation/access_token_store.h"
#include "chrome/browser/geolocation/location_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/common/geoposition.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "googleurl/src/gurl.h"

namespace {
const char* kDefaultNetworkProviderUrl = "https://www.google.com/loc/json";
GeolocationArbitrator* g_instance_ = NULL;
// TODO(joth): Remove this global function pointer and update all tests to use
// an injected ProviderFactory class instead.
GeolocationArbitrator::LocationProviderFactoryFunction
    g_provider_factory_function_for_test = NULL;
}  // namespace

// To avoid oscillations, set this to twice the expected update interval of a
// a GPS-type location provider (in case it misses a beat) plus a little.
const int64 GeolocationArbitrator::kFixStaleTimeoutMilliseconds =
    11 * base::Time::kMillisecondsPerSecond;

class GeolocationArbitratorImpl
    : public GeolocationArbitrator,
      public LocationProviderBase::ListenerInterface,
      public NonThreadSafe {
 public:
  GeolocationArbitratorImpl(AccessTokenStore* access_token_store,
                            URLRequestContextGetter* context_getter,
                            GetTimeNow get_time_now,
                            ProviderFactory* provider_factory);
  virtual ~GeolocationArbitratorImpl();

  // GeolocationArbitrator
  virtual void AddObserver(GeolocationArbitrator::Delegate* delegate,
                           const UpdateOptions& update_options);
  virtual bool RemoveObserver(GeolocationArbitrator::Delegate* delegate);
  virtual Geoposition GetCurrentPosition();
  virtual void OnPermissionGranted(const GURL& requesting_frame);
  virtual bool HasPermissionBeenGranted() const;

  // ListenerInterface
  virtual void LocationUpdateAvailable(LocationProviderBase* provider);

  void OnAccessTokenStoresLoaded(
      AccessTokenStore::AccessTokenSet access_token_store);

 private:
  // Takes ownership of |provider| on entry; it will either be added to
  // |provider_vector| or deleted on error (e.g. it fails to start).
  void RegisterProvider(LocationProviderBase* provider,
                        ScopedVector<LocationProviderBase>* provider_vector);
  void StartProviders();

  // Returns true if |new_position| is an improvement over |old_position|.
  // Set |from_same_provider| to true if both the positions came from the same
  // provider.
  bool IsNewPositionBetter(const Geoposition& old_position,
                           const Geoposition& new_position,
                           bool from_same_provider) const;

  scoped_refptr<AccessTokenStore> access_token_store_;
  scoped_refptr<URLRequestContextGetter> context_getter_;
  GetTimeNow get_time_now_;
  scoped_refptr<ProviderFactory> provider_factory_;

  ScopedVector<LocationProviderBase> providers_;

  typedef std::map<GeolocationArbitrator::Delegate*, UpdateOptions> DelegateMap;
  DelegateMap observers_;

  // The current best estimate of our position.
  Geoposition position_;
  // The provider which supplied the current |position_|
  const LocationProviderBase* position_provider_;

  GURL most_recent_authorized_frame_;

  CancelableRequestConsumer request_consumer_;
};

class DefaultLocationProviderFactory
    : public GeolocationArbitratorImpl::ProviderFactory {
 public:
  virtual LocationProviderBase* NewNetworkLocationProvider(
      AccessTokenStore* access_token_store,
      URLRequestContextGetter* context,
      const GURL& url,
      const string16& access_token) {
    if (g_provider_factory_function_for_test)
      return g_provider_factory_function_for_test();
    return ::NewNetworkLocationProvider(access_token_store, context,
                                        url, access_token);
  }
  virtual LocationProviderBase* NewSystemLocationProvider() {
    if (g_provider_factory_function_for_test)
      return NULL;
    return ::NewSystemLocationProvider();
  }
};

GeolocationArbitratorImpl::GeolocationArbitratorImpl(
    AccessTokenStore* access_token_store,
    URLRequestContextGetter* context_getter,
    GetTimeNow get_time_now,
    ProviderFactory* provider_factory)
    : access_token_store_(access_token_store),
      context_getter_(context_getter),
      get_time_now_(get_time_now),
      provider_factory_(provider_factory),
      position_provider_(NULL) {
  DCHECK(NULL == g_instance_);
  DCHECK(GURL(kDefaultNetworkProviderUrl).is_valid());
  g_instance_ = this;
  access_token_store_->LoadAccessTokens(
      &request_consumer_,
      NewCallback(this,
                  &GeolocationArbitratorImpl::OnAccessTokenStoresLoaded));
}

GeolocationArbitratorImpl::~GeolocationArbitratorImpl() {
  DCHECK(CalledOnValidThread());
  DCHECK(observers_.empty()) << "Not all observers have unregistered";
  DCHECK(this == g_instance_);
  g_instance_ = NULL;
}

void GeolocationArbitratorImpl::AddObserver(
    GeolocationArbitrator::Delegate* delegate,
    const UpdateOptions& update_options) {
  DCHECK(CalledOnValidThread());
  observers_[delegate] = update_options;
  StartProviders();
  if (position_.IsInitialized()) {
    delegate->OnLocationUpdate(position_);
  }
}

bool GeolocationArbitratorImpl::RemoveObserver(
    GeolocationArbitrator::Delegate* delegate) {
  DCHECK(CalledOnValidThread());
  size_t remove = observers_.erase(delegate);
  if (observers_.empty()) {
    // TODO(joth): Delayed callback to linger before stopping providers.
    for (ScopedVector<LocationProviderBase>::iterator i = providers_.begin();
        i != providers_.end(); ++i) {
      (*i)->StopProvider();
    }
  } else {  // The high accuracy requirement may have changed.
    StartProviders();
  }
  return remove > 0;
}

Geoposition GeolocationArbitratorImpl::GetCurrentPosition() {
  return position_;
}

void GeolocationArbitratorImpl::OnPermissionGranted(
    const GURL& requesting_frame) {
  DCHECK(CalledOnValidThread());
  most_recent_authorized_frame_ = requesting_frame;
  for (ScopedVector<LocationProviderBase>::iterator i = providers_.begin();
      i != providers_.end(); ++i) {
    (*i)->OnPermissionGranted(requesting_frame);
  }
}

bool GeolocationArbitratorImpl::HasPermissionBeenGranted() const {
  DCHECK(CalledOnValidThread());
  return most_recent_authorized_frame_.is_valid();
}

void GeolocationArbitratorImpl::LocationUpdateAvailable(
    LocationProviderBase* provider) {
  DCHECK(CalledOnValidThread());
  DCHECK(provider);
  Geoposition new_position;
  provider->GetPosition(&new_position);
  DCHECK(new_position.IsInitialized());
  if (!IsNewPositionBetter(position_, new_position,
                           provider == position_provider_))
    return;
  position_ = new_position;
  position_provider_ = provider;
  DelegateMap::const_iterator it = observers_.begin();
  while (it != observers_.end()) {
    // Advance iterator before callback to guard against synchronous unregister.
    Delegate* delegate = it->first;
    ++it;
    delegate->OnLocationUpdate(position_);
  }
}

void GeolocationArbitratorImpl::OnAccessTokenStoresLoaded(
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
        provider_factory_->NewNetworkLocationProvider(
            access_token_store_.get(), context_getter_.get(),
            i->first, i->second),
        &providers_);
  }
  RegisterProvider(provider_factory_->NewSystemLocationProvider(),
                   &providers_);
  StartProviders();
}

void GeolocationArbitratorImpl::RegisterProvider(
    LocationProviderBase* provider,
    ScopedVector<LocationProviderBase>* provider_vector) {
  DCHECK(provider_vector);
  if (!provider)
    return;
  provider->RegisterListener(this);
  if (most_recent_authorized_frame_.is_valid())
    provider->OnPermissionGranted(most_recent_authorized_frame_);
  provider_vector->push_back(provider);
}

void GeolocationArbitratorImpl::StartProviders() {
  DCHECK(CalledOnValidThread());
  const bool high_accuracy_required =
      UpdateOptions::Collapse(observers_).use_high_accuracy;
  for (ScopedVector<LocationProviderBase>::iterator i = providers_.begin();
      i != providers_.end(); ++i) {
    (*i)->StartProvider(high_accuracy_required);
  }
}

bool GeolocationArbitratorImpl::IsNewPositionBetter(
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

GeolocationArbitrator* GeolocationArbitrator::Create(
    AccessTokenStore* access_token_store,
    URLRequestContextGetter* context_getter,
    GetTimeNow get_time_now,
    ProviderFactory* provider_factory) {
  return new GeolocationArbitratorImpl(access_token_store, context_getter,
                                       get_time_now, provider_factory);
}

GeolocationArbitrator* GeolocationArbitrator::GetInstance() {
  if (!g_instance_) {
    // Construct the arbitrator using default token store and url context. We
    // get the url context getter from the default profile as it's not
    // particularly important which profile it is attached to: the network
    // request implementation disables cookies anyhow.
    Create(NewChromePrefsAccessTokenStore(),
           Profile::GetDefaultRequestContext(),
           base::Time::Now,
           new DefaultLocationProviderFactory);
    DCHECK(g_instance_);
  }
  return g_instance_;
}

void GeolocationArbitrator::SetProviderFactoryForTest(
      LocationProviderFactoryFunction factory_function) {
  g_provider_factory_function_for_test = factory_function;
}

GeolocationArbitrator::GeolocationArbitrator() {
}

GeolocationArbitrator::~GeolocationArbitrator() {
}

GeolocationArbitrator::ProviderFactory::~ProviderFactory() {
}
