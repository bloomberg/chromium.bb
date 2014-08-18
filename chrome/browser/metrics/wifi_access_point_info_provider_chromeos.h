// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_WIFI_ACCESS_POINT_INFO_PROVIDER_CHROMEOS_H_
#define CHROME_BROWSER_METRICS_WIFI_ACCESS_POINT_INFO_PROVIDER_CHROMEOS_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/metrics/wifi_access_point_info_provider.h"
#include "chromeos/network/network_state_handler_observer.h"

// WifiAccessPointInfoProviderChromeos provides the connected wifi
// acccess point information for chromeos.
class WifiAccessPointInfoProviderChromeos
    : public WifiAccessPointInfoProvider,
      public chromeos::NetworkStateHandlerObserver,
      public base::SupportsWeakPtr<WifiAccessPointInfoProviderChromeos> {
 public:
  WifiAccessPointInfoProviderChromeos();
  virtual ~WifiAccessPointInfoProviderChromeos();

  // WifiAccessPointInfoProvider
  virtual bool GetInfo(WifiAccessPointInfo* info) OVERRIDE;

  // NetworkStateHandlerObserver overrides.
  virtual void DefaultNetworkChanged(
      const chromeos::NetworkState* default_network) OVERRIDE;

 private:
  // Callback from Shill.Service.GetProperties. Parses |properties| to obtain
  // the wifi access point information.
  void ParseInfo(const std::string& service_path,
                 const base::DictionaryValue& properties);

  WifiAccessPointInfo wifi_access_point_info_;

  DISALLOW_COPY_AND_ASSIGN(WifiAccessPointInfoProviderChromeos);
};

#endif  // CHROME_BROWSER_METRICS_WIFI_ACCESS_POINT_INFO_PROVIDER_CHROMEOS_H_
