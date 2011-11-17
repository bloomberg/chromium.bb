// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_
#define CONTENT_BROWSER_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_
#pragma once

#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/threading/thread.h"
#include "content/browser/geolocation/device_data_provider.h"
#include "content/browser/geolocation/location_provider.h"
#include "content/browser/geolocation/network_location_request.h"
#include "content/common/content_export.h"
#include "content/common/geoposition.h"

class NetworkLocationProvider
    : public LocationProviderBase,
      public RadioDataProvider::ListenerInterface,
      public WifiDataProvider::ListenerInterface,
      public NetworkLocationRequest::ListenerInterface {
 public:
  // Cache of recently resolved locations. Public for tests.
  class CONTENT_EXPORT PositionCache {
   public:
    // The maximum size of the cache of positions for previously requested
    // device data.
    static const size_t kMaximumSize;

    PositionCache();
    ~PositionCache();

    // Caches the current position response for the current set of cell ID and
    // WiFi data. In the case of the cache exceeding kMaximumSize this will
    // evict old entries in FIFO orderer of being added.
    // Returns true on success, false otherwise.
    bool CachePosition(const WifiData& wifi_data,
                       const Geoposition& position);

    // Searches for a cached position response for the current set of device
    // data. Returns NULL if the position is not in the cache, or the cached
    // position if available. Ownership remains with the cache.
    const Geoposition* FindPosition(const WifiData& wifi_data);

   private:
    // Makes the key for the map of cached positions, using a set of
    // device data. Returns true if a good key was generated, false otherwise.
    static bool MakeKey(const WifiData& wifi_data,
                        string16* key);

    // The cache of positions. This is stored as a map keyed on a string that
    // represents a set of device data, and a list to provide
    // least-recently-added eviction.
    typedef std::map<string16, Geoposition> CacheMap;
    CacheMap cache_;
    typedef std::list<CacheMap::iterator> CacheAgeList;
    CacheAgeList cache_age_list_;  // Oldest first.
  };

  NetworkLocationProvider(AccessTokenStore* access_token_store,
                          net::URLRequestContextGetter* context,
                          const GURL& url,
                          const string16& access_token);
  virtual ~NetworkLocationProvider();

  // LocationProviderBase implementation
  virtual bool StartProvider(bool high_accuracy) OVERRIDE;
  virtual void StopProvider() OVERRIDE;
  virtual void GetPosition(Geoposition *position) OVERRIDE;
  virtual void UpdatePosition() OVERRIDE;
  virtual void OnPermissionGranted(const GURL& requesting_frame) OVERRIDE;

 private:
  // Satisfies a position request from cache or network.
  void RequestPosition();

  // Internal helper used by DeviceDataUpdateAvailable
  void OnDeviceDataUpdated();

  bool IsStarted() const;

  // DeviceDataProvider::ListenerInterface implementation.
  virtual void DeviceDataUpdateAvailable(RadioDataProvider* provider) OVERRIDE;
  virtual void DeviceDataUpdateAvailable(WifiDataProvider* provider) OVERRIDE;

  // NetworkLocationRequest::ListenerInterface implementation.
  virtual void LocationResponseAvailable(const Geoposition& position,
                                         bool server_error,
                                         const string16& access_token,
                                         const RadioData& radio_data,
                                         const WifiData& wifi_data) OVERRIDE;

  scoped_refptr<AccessTokenStore> access_token_store_;

  // The device data providers, acquired via global factories.
  RadioDataProvider* radio_data_provider_;
  WifiDataProvider* wifi_data_provider_;

  // The radio and wifi data, flags to indicate if each data set is complete.
  RadioData radio_data_;
  WifiData wifi_data_;
  bool is_radio_data_complete_;
  bool is_wifi_data_complete_;

  // The timestamp for the latest device data update.
  base::Time device_data_updated_timestamp_;

  // Cached value loaded from the token store or set by a previous server
  // response, and sent in each subsequent network request.
  string16 access_token_;

  // The current best position estimate.
  Geoposition position_;

  bool is_new_data_available_;

  std::string most_recent_authorized_host_;

  // The network location request object, and the url it uses.
  scoped_ptr<NetworkLocationRequest> request_;

  base::WeakPtrFactory<NetworkLocationProvider> weak_factory_;
  // The cache of positions.
  scoped_ptr<PositionCache> position_cache_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLocationProvider);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_
