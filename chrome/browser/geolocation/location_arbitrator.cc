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
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/browser/geolocation/access_token_store.h"
#include "chrome/browser/geolocation/location_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/common/geoposition.h"
#include "googleurl/src/gurl.h"

namespace {
const char* kDefaultNetworkProviderUrl = "https://www.google.com/loc/json";
GeolocationArbitrator* g_instance_ = NULL;
GeolocationArbitrator::LocationProviderFactoryFunction
    g_provider_factory_function_for_test = NULL;
}  // namespace

class GeolocationArbitratorImpl
    : public GeolocationArbitrator,
      public LocationProviderBase::ListenerInterface,
      public NonThreadSafe {
 public:
  GeolocationArbitratorImpl(AccessTokenStore* access_token_store,
                            URLRequestContextGetter* context_getter);
  virtual ~GeolocationArbitratorImpl();

  // GeolocationArbitrator
  virtual void AddObserver(GeolocationArbitrator::Delegate* delegate,
                           const UpdateOptions& update_options);
  virtual bool RemoveObserver(GeolocationArbitrator::Delegate* delegate);

  // ListenerInterface
  virtual void LocationUpdateAvailable(LocationProviderBase* provider);
  virtual void MovementDetected(LocationProviderBase* provider);

  void OnAccessTokenStoresLoaded(
      AccessTokenStore::AccessTokenSet access_token_store);

 private:
  void CreateProviders();
  bool HasHighAccuracyObserver();

  scoped_refptr<AccessTokenStore> access_token_store_;
  scoped_refptr<URLRequestContextGetter> context_getter_;

  const GURL default_url_;
  scoped_ptr<LocationProviderBase> provider_;

  typedef std::map<GeolocationArbitrator::Delegate*, UpdateOptions> DelegateMap;
  DelegateMap observers_;

  // The current best estimate of our position.
  Geoposition position_;

  CancelableRequestConsumer request_consumer_;
};

GeolocationArbitratorImpl::GeolocationArbitratorImpl(
    AccessTokenStore* access_token_store,
    URLRequestContextGetter* context_getter)
    : access_token_store_(access_token_store),
      context_getter_(context_getter),
      default_url_(kDefaultNetworkProviderUrl) {
  DCHECK(default_url_.is_valid());
  DCHECK(NULL == g_instance_);
  g_instance_ = this;
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
  if (provider_ == NULL) {
    CreateProviders();
  } else if (position_.IsInitialized()) {
    delegate->OnLocationUpdate(position_);
  }
}

bool GeolocationArbitratorImpl::RemoveObserver(
    GeolocationArbitrator::Delegate* delegate) {
  DCHECK(CalledOnValidThread());
  size_t remove = observers_.erase(delegate);
  if (observers_.empty() && provider_ != NULL) {
    // TODO(joth): Delayed callback to linger before destroying the provider.
    provider_->UnregisterListener(this);
    provider_.reset();
  }
  return remove > 0;
}

void GeolocationArbitratorImpl::LocationUpdateAvailable(
    LocationProviderBase* provider) {
  DCHECK(CalledOnValidThread());
  DCHECK(provider);
  provider->GetPosition(&position_);
  DCHECK(position_.IsInitialized());
  // TODO(joth): Arbitrate.
  DelegateMap::const_iterator it = observers_.begin();
  while (it != observers_.end()) {
    // Advance iterator before callback to guard against synchronous deregister.
    Delegate* delegate = it->first;
    ++it;
    delegate->OnLocationUpdate(position_);
  }
}

void GeolocationArbitratorImpl::MovementDetected(
    LocationProviderBase* provider) {
  DCHECK(CalledOnValidThread());
}

void GeolocationArbitratorImpl::OnAccessTokenStoresLoaded(
    AccessTokenStore::AccessTokenSet access_token_set) {
  DCHECK(provider_ == NULL)
      << "OnAccessTokenStoresLoaded : has existing location "
      << "provider. Race condition caused repeat load of tokens?";
  if (g_provider_factory_function_for_test) {
    provider_.reset(g_provider_factory_function_for_test());
  } else {
    // TODO(joth): Once we have arbitration implementation, iterate the whole
    // set rather than cherry-pick our defaul url.
    const AccessTokenStore::AccessTokenSet::const_iterator token =
        access_token_set.find(default_url_);
    // TODO(joth): Follow up with GLS folks if they have a plan for replacing
    // the hostname field. Sending chromium.org is a stop-gap.
    provider_.reset(NewNetworkLocationProvider(
        access_token_store_.get(), context_getter_.get(), default_url_,
        token != access_token_set.end() ? token->second : string16(),
        ASCIIToUTF16("chromium.org")));
  }
  DCHECK(provider_ != NULL);
  provider_->RegisterListener(this);
  const bool ok = provider_->StartProvider();
  DCHECK(ok);
}

void GeolocationArbitratorImpl::CreateProviders() {
  DCHECK(CalledOnValidThread());
  DCHECK(!observers_.empty());
  DCHECK(provider_ == NULL);
  if (!request_consumer_.HasPendingRequests()) {
    access_token_store_->LoadAccessTokens(
        &request_consumer_,
        NewCallback(this,
                    &GeolocationArbitratorImpl::OnAccessTokenStoresLoaded));
  }
  // TODO(joth): Use high accuracy flag to conditionally create GPS provider.
}

bool GeolocationArbitratorImpl::HasHighAccuracyObserver() {
  DCHECK(CalledOnValidThread());
  for (DelegateMap::const_iterator it = observers_.begin();
      it != observers_.end(); ++it) {
    if (it->second.use_high_accuracy)
      return true;
  }
  return false;
}

GeolocationArbitrator* GeolocationArbitrator::Create(
    AccessTokenStore* access_token_store,
    URLRequestContextGetter* context_getter) {
  DCHECK(!g_instance_);
  return new GeolocationArbitratorImpl(access_token_store, context_getter);
}

GeolocationArbitrator* GeolocationArbitrator::GetInstance() {
  if (!g_instance_) {
    // Construct the arbitrator using default token store and url context. We
    // get the url context getter from the default profile as it's not
    // particularly important which profile it is attached to: the network
    // request implementation disables cookies anyhow.
    Create(NewChromePrefsAccessTokenStore(),
           Profile::GetDefaultRequestContext());
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
