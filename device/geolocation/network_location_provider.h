// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_
#define DEVICE_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "device/geolocation/geolocation_export.h"
#include "device/geolocation/geoposition.h"
#include "device/geolocation/location_provider.h"
#include "device/geolocation/network_location_request.h"
#include "device/geolocation/wifi_data_provider_manager.h"

namespace device {

class NetworkLocationProvider : public LocationProvider {
 public:
  // Cache of recently resolved locations. Public for tests.
  class DEVICE_GEOLOCATION_EXPORT PositionCache {
   public:
    // The maximum size of the cache of positions.
    static const size_t kMaximumSize;

    PositionCache();
    ~PositionCache();

    // Caches the current position response for the current set of cell ID and
    // WiFi data. In the case of the cache exceeding kMaximumSize this will
    // evict old entries in FIFO orderer of being added.
    // Returns true on success, false otherwise.
    bool CachePosition(const WifiData& wifi_data, const Geoposition& position);

    // Searches for a cached position response for the current set of data.
    // Returns NULL if the position is not in the cache, or the cached
    // position if available. Ownership remains with the cache.
    const Geoposition* FindPosition(const WifiData& wifi_data);

   private:
    // Makes the key for the map of cached positions, using a set of
    // data. Returns true if a good key was generated, false otherwise.
    static bool MakeKey(const WifiData& wifi_data, base::string16* key);

    // The cache of positions. This is stored as a map keyed on a string that
    // represents a set of data, and a list to provide
    // least-recently-added eviction.
    typedef std::map<base::string16, Geoposition> CacheMap;
    CacheMap cache_;
    typedef std::list<CacheMap::iterator> CacheAgeList;
    CacheAgeList cache_age_list_;  // Oldest first.
  };

  DEVICE_GEOLOCATION_EXPORT NetworkLocationProvider(
      scoped_refptr<net::URLRequestContextGetter> context,
      const std::string& api_key);
  ~NetworkLocationProvider() override;

  // LocationProvider implementation
  void SetUpdateCallback(const LocationProviderUpdateCallback& cb) override;
  bool StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const Geoposition& GetPosition() override;
  void OnPermissionGranted() override;

 private:
  // Tries to update |position_| request from cache or network.
  void RequestPosition();

  // Gets called when new wifi data is available, either via explicit request to
  // or callback from |wifi_data_provider_manager_|.
  void OnWifiDataUpdate();

  bool IsStarted() const;

  void OnLocationResponse(const Geoposition& position,
                          bool server_error,
                          const WifiData& wifi_data);

  // The wifi data provider, acquired via global factories. Valid between
  // StartProvider() and StopProvider(), and checked via IsStarted().
  WifiDataProviderManager* wifi_data_provider_manager_;

  WifiDataProviderManager::WifiDataUpdateCallback wifi_data_update_callback_;

  // The  wifi data and a flag to indicate if the data set is complete.
  WifiData wifi_data_;
  bool is_wifi_data_complete_;

  // The timestamp for the latest wifi data update.
  base::Time wifi_timestamp_;

  // The current best position estimate.
  Geoposition position_;

  LocationProvider::LocationProviderUpdateCallback
      location_provider_update_callback_;

  // Whether permission has been granted for the provider to operate.
  bool is_permission_granted_;

  bool is_new_data_available_;

  // The network location request object.
  const std::unique_ptr<NetworkLocationRequest> request_;

  // The cache of positions.
  const std::unique_ptr<PositionCache> position_cache_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<NetworkLocationProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLocationProvider);
};

}  // namespace device

#endif  // DEVICE_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_
