// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_WIFI_WIFI_SERVICE_H_
#define CHROME_UTILITY_WIFI_WIFI_SERVICE_H_

#include <list>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/values.h"
#include "components/wifi/wifi_export.h"

namespace wifi {

// WiFiService interface used by implementation of chrome.networkingPrivate
// JavaScript extension API. All methods should be called on worker thread.
// It could be created on any (including UI) thread, so nothing expensive should
// be done in the constructor.
class WIFI_EXPORT WiFiService {
 public:
  typedef std::vector<std::string> NetworkGuidList;
  typedef base::Callback<
      void(const NetworkGuidList& network_guid_list)> NetworkGuidListCallback;

  virtual ~WiFiService() {}

  // Create instance of |WiFiService| for normal use.
  static WiFiService* Create();
  // Create instance of |WiFiService| for unit test use.
  static WiFiService* CreateForTest();

  // Get Properties of network identified by |network_guid|. Populates
  // |properties| on success, |error| on failure.
  virtual void GetProperties(const std::string& network_guid,
                             DictionaryValue* properties,
                             std::string* error) = 0;

  // Set Properties of network identified by |network_guid|. Populates |error|
  // on failure.
  virtual void SetProperties(const std::string& network_guid,
                             scoped_ptr<base::DictionaryValue> properties,
                             std::string* error) = 0;

  // Get list of visible networks. Populates |network_list| on success.
  virtual void GetVisibleNetworks(ListValue* network_list) = 0;

  // Request network scan. Send |NetworkListChanged| event on completion.
  virtual void RequestNetworkScan() = 0;

  // Start connect to network identified by |network_guid|. Populates |error|
  // on failure.
  virtual void StartConnect(const std::string& network_guid,
                            std::string* error) = 0;

  // Start disconnect from network identified by |network_guid|. Populates
  // |error| on failure.
  virtual void StartDisconnect(const std::string& network_guid,
                               std::string* error) = 0;

  // Set observers to run when |NetworksChanged| and |NetworksListChanged|
  // events needs to be sent. Notifications are posted on |message_loop_proxy|.
  virtual void SetEventObservers(
      scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
      const NetworkGuidListCallback& networks_changed_observer,
      const NetworkGuidListCallback& network_list_changed_observer) = 0;

 protected:
  WiFiService() {}

  typedef int32 Frequency;
  enum FrequencyEnum {
    kFrequencyUnknown = 0,
    kFrequency2400 = 2400,
    kFrequency5000 = 5000
  };

  typedef std::list<Frequency> FrequencyList;
  // Network Properties, used as result of |GetProperties| and
  // |GetVisibleNetworks|.
  struct WIFI_EXPORT NetworkProperties {
    NetworkProperties();
    ~NetworkProperties();

    std::string connection_state;
    std::string guid;
    std::string name;
    std::string ssid;
    std::string bssid;
    std::string type;
    std::string security;
    // WiFi Signal Strength. 0..100
    uint32 signal_strength;
    bool auto_connect;
    Frequency frequency;
    FrequencyList frequency_list;

    std::string json_extra;  // Extra JSON properties for unit tests

    scoped_ptr<base::DictionaryValue> ToValue(bool network_list) const;
    bool UpdateFromValue(const base::DictionaryValue& value);
    static std::string MacAddressAsString(const uint8 mac_as_int[6]);
    static bool OrderByType(const NetworkProperties& l,
                            const NetworkProperties& r);
  };

  typedef std::list<NetworkProperties> NetworkList;

 private:
  DISALLOW_COPY_AND_ASSIGN(WiFiService);
};

}  // namespace wifi

#endif  // CHROME_UTILITY_WIFI_WIFI_SERVICE_H_
