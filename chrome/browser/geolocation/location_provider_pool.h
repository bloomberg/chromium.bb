// Copyright 2008, Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GEARS_GEOLOCATION_LOCATION_PROVIDER_POOL_H__
#define GEARS_GEOLOCATION_LOCATION_PROVIDER_POOL_H__

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
  LocationProviderBase *Register(
      BrowsingContext *browsing_context,
      const std::string16 &type,
      const std::string16 &url,
      const std::string16 &host,
      bool request_address,
      const std::string16 &language,
      LocationProviderBase::ListenerInterface *listener);

  // Unregister a listener from a given location provider. Deletes the provider
  // if there are no remaining listeners registered with it. Return value
  // indicates whether the provider was found in the pool.
  bool Unregister(LocationProviderBase *provider,
                  LocationProviderBase::ListenerInterface *listener);

  // Configures the pool to return a mock location provider for the type "MOCK".
  // This method is used only by the Gears test object.
  void UseMockLocationProvider(bool use_mock_location_provider);

  LocationProviderBase *NewProvider(BrowsingContext *browsing_context,
                                    const std::string16 &type,
                                    const std::string16 &url,
                                    const std::string16 &host,
                                    const std::string16 &language);

 private:
  static LocationProviderPool instance_;

  typedef std::pair<LocationProviderBase*, RefCount*> ProviderPair;
  typedef std::map<std::string16, ProviderPair> ProviderMap;
  ProviderMap providers_;
  Mutex providers_mutex_;

  bool use_mock_location_provider_;

  DISALLOW_EVIL_CONSTRUCTORS(LocationProviderPool);
};

#endif  // if 0

#endif  // GEARS_GEOLOCATION_LOCATION_PROVIDER_POOL_H__
