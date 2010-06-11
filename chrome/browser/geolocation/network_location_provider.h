// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_
#define CHROME_BROWSER_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_

#include <string>

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/thread.h"
#include "chrome/browser/geolocation/device_data_provider.h"
#include "chrome/browser/geolocation/location_provider.h"
#include "chrome/browser/geolocation/network_location_request.h"
#include "chrome/common/geoposition.h"

class URLFetcherProtectEntry;

class NetworkLocationProvider
    : public LocationProviderBase,
      public RadioDataProvider::ListenerInterface,
      public WifiDataProvider::ListenerInterface,
      public NetworkLocationRequest::ListenerInterface {
 public:
  NetworkLocationProvider(AccessTokenStore* access_token_store,
                          URLRequestContextGetter* context,
                          const GURL& url,
                          const string16& access_token);
  virtual ~NetworkLocationProvider();

  // LocationProviderBase implementation
  virtual bool StartProvider(bool high_accuracy);
  virtual void StopProvider();
  virtual void GetPosition(Geoposition *position);
  virtual void UpdatePosition();
  virtual void OnPermissionGranted(const GURL& requesting_frame);

 private:
  // PositionCache is an implementation detail of NetworkLocationProvider.
  class PositionCache;

  // Satisfies a position request from cache or network.
  void RequestPosition();

  // Internal helper used by DeviceDataUpdateAvailable
  void OnDeviceDataUpdated();

  bool IsStarted() const;

  // DeviceDataProvider::ListenerInterface implementation.
  virtual void DeviceDataUpdateAvailable(RadioDataProvider* provider);
  virtual void DeviceDataUpdateAvailable(WifiDataProvider* provider);

  // NetworkLocationRequest::ListenerInterface implementation.
  virtual void LocationResponseAvailable(const Geoposition& position,
                                         bool server_error,
                                         const string16& access_token,
                                         const RadioData& radio_data,
                                         const WifiData& wifi_data);

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

  ScopedRunnableMethodFactory<NetworkLocationProvider> delayed_start_task_;
  // The cache of positions.
  scoped_ptr<PositionCache> position_cache_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLocationProvider);
};

#endif  // CHROME_BROWSER_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_
