// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_LOCATION_PROVIDER_POOL_H_
#define CHROME_BROWSER_GEOLOCATION_LOCATION_PROVIDER_POOL_H_

// TODO(joth): port to chromium
#if 0

#include <map>
#include "gears/base/common/common.h"
#include "gears/base/common/mutex.h"
#include "gears/base/common/scoped_refptr.h"  // For RefCount
#include "gears/base/common/string16.h"
#include "gears/geolocation/location_provider.h"

class LocationProviderPool {
 public:
  LocationProviderPool();
  ~LocationProviderPool();

  static LocationProviderPool *GetInstance();

  // Registers a listener with a given type of location provider. Creates the
  // provider if the pool does not alrerady contain an instance of that
  // provider. Returns the provider, or NULL if it cannot be created.
  LocationProviderBase* Register(
      BrowsingContext* browsing_context,
      const string16& type,
      const string16& url,
      const string16& host,
      bool request_address,
      const string16& language,
      LocationProviderBase::ListenerInterface *listener);

  // Unregister a listener from a given location provider. Deletes the provider
  // if there are no remaining listeners registered with it. Return value
  // indicates whether the provider was found in the pool.
  bool Unregister(LocationProviderBase* provider,
                  LocationProviderBase::ListenerInterface* listener);

  // Configures the pool to return a mock location provider for the type "MOCK".
  // This method is used only by the Gears test object.
  void UseMockLocationProvider(bool use_mock_location_provider);

  LocationProviderBase *NewProvider(BrowsingContext* browsing_context,
                                    const string16& type,
                                    const string16& url,
                                    const string16& host,
                                    const string16& language);

 private:
  static LocationProviderPool instance_;

  typedef std::pair<LocationProviderBase*, RefCount*> ProviderPair;
  typedef std::map<string16, ProviderPair> ProviderMap;
  ProviderMap providers_;
  Mutex providers_mutex_;

  bool use_mock_location_provider_;

  DISALLOW_EVIL_CONSTRUCTORS(LocationProviderPool);
};

#endif  // if 0

#endif  // CHROME_BROWSER_GEOLOCATION_LOCATION_PROVIDER_POOL_H_
