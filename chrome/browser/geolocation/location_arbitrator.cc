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
#include "chrome/browser/geolocation/geoposition.h"
#include "chrome/browser/geolocation/location_provider.h"
#include "googleurl/src/gurl.h"

namespace {
const char* kDefaultNetworkProviderUrl = "https://www.google.com/loc/json";
}  // namespace

class GeolocationArbitratorImpl
    : public GeolocationArbitrator,
      public LocationProviderBase::ListenerInterface,
      public AccessTokenStoreFactory::Delegate,
      public NonThreadSafe {
 public:
  GeolocationArbitratorImpl(AccessTokenStoreFactory* access_token_store_factory,
                            URLRequestContextGetter* context_getter);
  ~GeolocationArbitratorImpl();

  // GeolocationArbitrator
  virtual void AddObserver(GeolocationArbitrator::Delegate* delegate,
                           const UpdateOptions& update_options);
  virtual bool RemoveObserver(GeolocationArbitrator::Delegate* delegate);
  virtual void SetUseMockProvider(bool use_mock);

  // ListenerInterface
  virtual void LocationUpdateAvailable(LocationProviderBase* provider);
  virtual void MovementDetected(LocationProviderBase* provider);

  // AccessTokenStoreFactory::Delegate
  virtual void OnAccessTokenStoresCreated(
      const AccessTokenStoreFactory::TokenStoreSet& access_token_store);

 private:
  void CreateProviders();
  bool HasHighAccuracyObserver();

  scoped_refptr<AccessTokenStoreFactory> access_token_store_factory_;
  scoped_refptr<URLRequestContextGetter> context_getter_;

  const GURL default_url_;
  scoped_ptr<LocationProviderBase> provider_;

  typedef std::map<GeolocationArbitrator::Delegate*, UpdateOptions> DelegateMap;
  DelegateMap observers_;

  base::WeakPtrFactory<GeolocationArbitratorImpl> weak_ptr_factory_;

  bool use_mock_;
};

GeolocationArbitratorImpl::GeolocationArbitratorImpl(
    AccessTokenStoreFactory* access_token_store_factory,
    URLRequestContextGetter* context_getter)
    : access_token_store_factory_(access_token_store_factory),
      context_getter_(context_getter),
      default_url_(kDefaultNetworkProviderUrl),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
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
  Position position;
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

void GeolocationArbitratorImpl::OnAccessTokenStoresCreated(
    const AccessTokenStoreFactory::TokenStoreSet& access_token_store) {
  DCHECK(provider_ == NULL);
  // TODO(joth): Once we have arbitration implementation, iterate the whole set
  // rather than cherry-pick our defaul url.
  AccessTokenStoreFactory::TokenStoreSet::const_iterator item =
      access_token_store.find(default_url_);
  DCHECK(item != access_token_store.end());
  DCHECK(item->second != NULL);
  if (use_mock_) {
    provider_.reset(NewMockLocationProvider());
  } else {
    // TODO(joth): Check with GLS folks what hostname they'd like sent.
    provider_.reset(NewNetworkLocationProvider(
        item->second, context_getter_.get(),
        default_url_, ASCIIToUTF16("chromium.org")));
  }
  DCHECK(provider_ != NULL);
  provider_->RegisterListener(this);
  const bool ok = provider_->StartProvider();
  DCHECK(ok);
}

void GeolocationArbitratorImpl::CreateProviders() {
  DCHECK(CalledOnValidThread());
  DCHECK(!observers_.empty());
  if (provider_ == NULL) {
    access_token_store_factory_->CreateAccessTokenStores(
        weak_ptr_factory_.GetWeakPtr(), default_url_);
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
    AccessTokenStoreFactory* access_token_store_factory,
    URLRequestContextGetter* context_getter) {
  return new GeolocationArbitratorImpl(access_token_store_factory,
                                       context_getter);
}

GeolocationArbitrator::~GeolocationArbitrator() {
}
