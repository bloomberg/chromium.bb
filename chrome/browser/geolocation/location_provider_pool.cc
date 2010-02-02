// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(joth): port to chromium
#if 0

#include "gears/geolocation/location_provider_pool.h"

#include <assert.h>

static const char16 *kMockString = STRING16(L"MOCK");
static const char16 *kGpsString = STRING16(L"GPS");
static const char16 *kNetworkString = STRING16(L"NETWORK");

// Local functions
static string16 MakeKey(const string16& type,
                        const string16& url,
                        const string16& host,
                        const string16& language);

// static member variables
LocationProviderPool LocationProviderPool::instance_;

LocationProviderPool::LocationProviderPool()
    : use_mock_location_provider_(false) {
}

LocationProviderPool::~LocationProviderPool() {
#if BROWSER_IEMOBILE
  // The lack of unload monitoring on IE Mobile on WinCE means that we may leak
  // providers.
#else
  DCHECK(providers_.empty());
#endif  // BROWSER_IEMOBILE
}

// static
LocationProviderPool *LocationProviderPool::GetInstance() {
  return &instance_;
}

LocationProviderBase* LocationProviderPool::Register(
    BrowsingContext* browsing_context,
    const string16& type,
    const string16& url,
    const string16& host,
    bool request_address,
    const string16& language,
    LocationProviderBase::ListenerInterface* listener) {
  DCHECK(listener);
  MutexLock lock(&providers_mutex_);
  string16 key = MakeKey(type, url, host, language);
  ProviderMap::iterator iter = providers_.find(key);
  if (iter == providers_.end()) {
    LocationProviderBase *provider = NewProvider(browsing_context, type, url,
                                                 host, language);
    if (!provider) {
      return NULL;
    }
    std::pair<ProviderMap::iterator, bool> result =
        providers_.insert(
        std::make_pair(key,
        std::make_pair(provider, new RefCount())));
    DCHECK(result.second);
    iter = result.first;
  }
  LocationProviderBase *provider = iter->second.first;
  DCHECK(provider);
  provider->RegisterListener(listener, request_address);
  RefCount* count = iter->second.second;
  DCHECK(count);
  count->Ref();
  return provider;
}

bool LocationProviderPool::Unregister(
    LocationProviderBase* provider,
    LocationProviderBase::ListenerInterface* listener) {
  DCHECK(provider);
  DCHECK(listener);
  MutexLock lock(&providers_mutex_);
  for (ProviderMap::iterator iter = providers_.begin();
       iter != providers_.end();
       ++iter) {
    LocationProviderBase* current_provider = iter->second.first;
    if (current_provider == provider) {
      current_provider->UnregisterListener(listener);
      RefCount *count = iter->second.second;
      DCHECK(count);
      if (count->Unref()) {
        delete current_provider;
        delete count;
        providers_.erase(iter);
      }
      return true;
    }
  }
  return false;
}

void LocationProviderPool::UseMockLocationProvider(
    bool use_mock_location_provider) {
  use_mock_location_provider_ = use_mock_location_provider;
}

LocationProviderBase* LocationProviderPool::NewProvider(
    BrowsingContext* browsing_context,
    const string16& type,
    const string16& url,
    const string16& host,
    const string16& language) {
  if (type == kMockString) {
    // use_mock_location_provider_ can only be set to true in a build that uses
    // USING_CCTESTS.
#if USING_CCTESTS
    if (use_mock_location_provider_) {
      return NewMockLocationProvider();
    } else {
      return NULL;
    }
#else
    return NULL;
#endif  // USING_CCTESTS
  } else if (type == kGpsString) {
    return NewGpsLocationProvider(browsing_context, url, host, language);
  } else if (type == kNetworkString) {
    return NewNetworkLocationProvider(browsing_context, url, host, language);
  }
  DCHECK(false);
  return NULL;
}

// Local function
static string16 MakeKey(const string16& type,
                             const string16& url,
                             const string16& host,
                             const string16& language) {
  // Network requests are made from a specific host and use a specific language.
  // Therefore we must key network and GPS providers on server URL, host and
  // language.
  if (type == kMockString) {
    return type;
  } else if (type == kGpsString || type == kNetworkString) {
    string16 key = type;
    if (!url.empty()) {
      key += STRING16(L" url=") + url;
    }
    if (!host.empty()) {
      key += STRING16(L" host=") + host;
    }
    if (!language.empty()) {
      key += STRING16(L" language=") + language;
    }
    return key;
  }
  DCHECK(false);
  return STRING16(L"");
}

#endif  // if 0
