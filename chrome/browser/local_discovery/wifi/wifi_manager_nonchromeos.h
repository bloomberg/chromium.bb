// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_WIFI_MANAGER_NONCHROMEOS_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_WIFI_MANAGER_NONCHROMEOS_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/local_discovery/wifi/wifi_manager.h"
#include "content/public/browser/browser_thread.h"

namespace local_discovery {

namespace wifi {

class WifiManagerNonChromeos : public WifiManager {
 public:
  WifiManagerNonChromeos();
  ~WifiManagerNonChromeos() override;

  // WifiManager implementation.
  void Start() override;
  void GetSSIDList(const SSIDListCallback& callback) override;
  void RequestScan() override;
  void ConfigureAndConnectNetwork(const std::string& ssid,
                                  const WifiCredentials& credentials,
                                  const SuccessCallback& callback) override;
  void ConnectToNetworkByID(const std::string& internal_id,
                            const SuccessCallback& callback) override;
  void RequestNetworkCredentials(const std::string& ssid,
                                 const CredentialsCallback& callback) override;
  void AddNetworkListObserver(NetworkListObserver* observer) override;
  void RemoveNetworkListObserver(NetworkListObserver* observer) override;

 private:
  class WifiServiceWrapper;

  // Called when the network list changes. Used by WifiServiceWrapper.
  void OnNetworkListChanged(scoped_ptr<NetworkPropertiesList> ssid_list);

  // Used to post callbacks that take a const& network list without copying the
  // vector between threads.
  void PostSSIDListCallback(const SSIDListCallback& callback,
                            scoped_ptr<NetworkPropertiesList> ssid_list);

  // Used to ensure closures posted from the wifi threads aren't called after
  // the service client is deleted.
  void PostClosure(const base::Closure& callback);

  std::string original_guid_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  WifiServiceWrapper* wifi_wrapper_;  // Owned. Deleted on file thread.
  ObserverList<NetworkListObserver> network_list_observers_;

  base::WeakPtrFactory<WifiManagerNonChromeos> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WifiManagerNonChromeos);
};

}  // namespace wifi

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_WIFI_MANAGER_NONCHROMEOS_H_
