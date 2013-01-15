// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_GEOLOCATION_HANDLER_H_
#define CHROMEOS_NETWORK_GEOLOCATION_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "chromeos/network/network_util.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

// This class provices Shill Wifi Access Point data. It currently relies on
// polling because that is the usage model in content::WifiDataProvider. This
// class requests data asynchronously, returning the most recent available data.
// A typical usage pattern, assuming a wifi device is enabeld, is:
//   Initialize();  // Makes an initial request
//   GetWifiAccessPoints();  // returns true + inital data, requests update
//   (Delay some amount of time, ~10s)
//   GetWifiAccessPoints();  // returns true + updated data, requests update
//   (Delay some amount of time after data changed, ~10s)
//   GetWifiAccessPoints();  // returns true + same data, requests update
//   (Delay some amount of time after data did not change, ~2 mins)

class CHROMEOS_EXPORT GeolocationHandler : public ShillPropertyChangedObserver {
 public:
  ~GeolocationHandler();

  // Manage the global instance. Must be initialized before any calls to Get().
  static void Initialize();
  static void Shutdown();
  static GeolocationHandler* Get();

  // This sends a request for wifi access point data. If data is already
  // available, returns |true|, fills |access_points| with the latest access
  // point data, and sets |age_ms| to the time since the last update in MS.
  bool GetWifiAccessPoints(WifiAccessPointVector* access_points, int64* age_ms);

  bool wifi_enabled() const { return wifi_enabled_; }

  // ShillPropertyChangedObserver overrides
  virtual void OnPropertyChanged(const std::string& key,
                                 const base::Value& value) OVERRIDE;

 private:
  friend class GeolocationHandlerTest;
  GeolocationHandler();
  void Init();

  // ShillManagerClient callback
  void ManagerPropertiesCallback(DBusMethodCallStatus call_status,
                                 const base::DictionaryValue& properties);

  // Called from OnPropertyChanged or ManagerPropertiesCallback.
  void HandlePropertyChanged(const std::string& key, const base::Value& value);

  // Asynchronously request wifi access points from Shill.Manager.
  void RequestWifiAccessPoints();

  // Callback for receiving Geolocation data.
  void GeolocationCallback(DBusMethodCallStatus call_status,
                           const base::DictionaryValue& properties);

  // Wimax enabled state
  bool wifi_enabled_;

  // Cached wifi access points and update time
  WifiAccessPointVector wifi_access_points_;
  base::Time geolocation_received_time_;

  // For Shill client callbacks
  base::WeakPtrFactory<GeolocationHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_GEOLOCATION_HANDLER_H_
