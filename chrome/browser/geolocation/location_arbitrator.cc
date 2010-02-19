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
#include "chrome/common/geoposition.h"
#include "googleurl/src/gurl.h"

namespace {
const char* kDefaultNetworkProviderUrl = "https://www.google.com/loc/json";
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
  virtual void SetUseMockProvider(bool use_mock);

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

  CancelableRequestConsumer request_consumer_;
  bool use_mock_;
};

GeolocationArbitratorImpl::GeolocationArbitratorImpl(
    AccessTokenStore* access_token_store,
    URLRequestContextGetter* context_getter)
    : access_token_store_(access_token_store),
      context_getter_(context_getter),
      default_url_(kDefaultNetworkProviderUrl),
      use_mock_(false) {
  DCHECK(default_url_.is_valid());
}

GeolocationArbitratorImpl::~GeolocationArbitratorImpl() {
  DCHECK(CalledOnValidThread());
  DCHECK(observers_.empty()) << "Not all observers have unregistered";
}

void GeolocationArbitratorImpl::AddObserver(
    GeolocationArbitrator::Delegate* delegate,
    const UpdateOptions& update_options) {
  DCHECK(CalledOnValidThread());
  observers_[delegate] = update_options;
  CreateProviders();
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

void GeolocationArbitratorImpl::SetUseMockProvider(bool use_mock) {
  DCHECK(CalledOnValidThread());
  DCHECK(provider_ == NULL);
  use_mock_ = use_mock;
}

void GeolocationArbitratorImpl::LocationUpdateAvailable(
    LocationProviderBase* provider) {
  DCHECK(CalledOnValidThread());
  DCHECK(provider);
  Geoposition position;
  provider->GetPosition(&position);
  // TODO(joth): Arbitrate.
  for (DelegateMap::const_iterator it = observers_.begin();
      it != observers_.end(); ++it) {
    it->first->OnLocationUpdate(position);
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
  if (use_mock_) {
    provider_.reset(NewMockLocationProvider());
  } else {
    // TODO(joth): Once we have arbitration implementation, iterate the whole
    // set rather than cherry-pick our defaul url.
    const AccessTokenStore::AccessTokenSet::const_iterator token =
        access_token_set.find(default_url_);
    // TODO(joth): Check with GLS folks what hostname they'd like sent.
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
  if (provider_ == NULL && !request_consumer_.HasPendingRequests()) {
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

GeolocationArbitrator* GeolocationArbitrator::New(
    AccessTokenStore* access_token_store,
    URLRequestContextGetter* context_getter) {
  return new GeolocationArbitratorImpl(access_token_store,
                                       context_getter);
}

GeolocationArbitrator::~GeolocationArbitrator() {
}
