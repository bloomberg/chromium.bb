// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;
var assertTrue = chrome.test.assertTrue;
var assertFalse = chrome.test.assertFalse;
var assertEq = chrome.test.assertEq;

// Test properties for the verification API.
var verificationProperties = {
  "certificate": "certificate",
  "publicKey": "public_key",
  "nonce": "nonce",
  "signedData": "signed_data",
  "deviceSerial": "device_serial",
  "deviceSsid": "Device 0123",
  "deviceBssid": "00:01:02:03:04:05"
};

var privateHelpers = {
  // Watches for the states |expectedStates| in reverse order. If all states
  // were observed in the right order, succeeds and calls |done|. If any
  // unexpected state is observed, fails.
  watchForStateChanges: function(network, expectedStates, done) {
    var self = this;
    var collectProperties = function(properties) {
      var finishTest = function() {
        chrome.networkingPrivate.onNetworksChanged.removeListener(
            self.onNetworkChange);
        done();
      };
      if (expectedStates.length > 0) {
        var expectedState = expectedStates.pop();
        assertEq(expectedState, properties.ConnectionState);
        if (expectedStates.length == 0)
          finishTest();
      }
    };
    this.onNetworkChange = function(changes) {
      assertEq([network], changes);
      chrome.networkingPrivate.getProperties(
          network,
          callbackPass(collectProperties));
    };
    chrome.networkingPrivate.onNetworksChanged.addListener(
        this.onNetworkChange);
  },
  listListener: function(expected, done) {
    var self = this;
    this.listenForChanges = function(list) {
      assertEq(expected, list);
      chrome.networkingPrivate.onNetworkListChanged.removeListener(
          self.listenForChanges);
      done();
    };
  }
};

var availableTests = [
  function startConnect() {
    chrome.networkingPrivate.startConnect("stub_wifi2", callbackPass());
  },
  function startDisconnect() {
    // Must connect to a network before we can disconnect from it.
    chrome.networkingPrivate.startConnect("stub_wifi2", callbackPass(
      function() {
        chrome.networkingPrivate.startDisconnect("stub_wifi2", callbackPass());
      }));
  },
  function startConnectNonexistent() {
    chrome.networkingPrivate.startConnect(
      "nonexistent_path",
      callbackFail("configure-failed"));
  },
  function startDisconnectNonexistent() {
    chrome.networkingPrivate.startDisconnect(
      "nonexistent_path",
      callbackFail("not-found"));
  },
  function startGetPropertiesNonexistent() {
    chrome.networkingPrivate.getProperties(
      "nonexistent_path",
      callbackFail("Error.DBusFailed"));
  },
  function createNetwork() {
    chrome.networkingPrivate.createNetwork(
      false,  // shared
      { "Type": "WiFi",
        "GUID": "ignored_guid",
        "WiFi": {
          "SSID": "wifi_created",
          "Security": "WEP-PSK"
        }
      },
      callbackPass(function(guid) {
        assertFalse(guid == "");
        assertFalse(guid == "ignored_guid");
        chrome.networkingPrivate.getProperties(
          guid,
          callbackPass(function(properties) {
            assertEq("WiFi", properties.Type);
            assertEq(guid, properties.GUID);
            assertEq("wifi_created", properties.WiFi.SSID);
            assertEq("WEP-PSK", properties.WiFi.Security);
          }));
      }));
  },
  function getVisibleNetworks() {
    chrome.networkingPrivate.getVisibleNetworks(
      "All",
      callbackPass(function(result) {
        assertEq([{
                    "ConnectionState": "Connected",
                    "GUID": "stub_ethernet",
                    "Name": "eth0",
                    "Type": "Ethernet",
                    "Ethernet": {
                      "Authentication": "None"
                    }
                  },
                  {
                    "ConnectionState": "Connected",
                    "GUID": "stub_wifi1",
                    "Name": "wifi1",
                    "Type": "WiFi",
                    "WiFi": {
                      "Security": "WEP-PSK",
                      "SignalStrength": 0
                    }
                  },
                  {
                    "ConnectionState": "Connected",
                    "GUID": "stub_vpn1",
                    "Name": "vpn1",
                    "Type": "VPN",
                  },
                  {
                    "ConnectionState": "NotConnected",
                    "GUID": "stub_wifi2",
                    "Name": "wifi2_PSK",
                    "Type": "WiFi",
                    "WiFi": {
                      "Security": "WPA-PSK",
                      "SignalStrength": 80
                    }
                  },
                  {
                    "Cellular": {
                      "ActivateOverNonCellularNetwork": false,
                      "ActivationState": "not-activated",
                      "NetworkTechnology": "GSM",
                      "RoamingState": "home"
                    },
                    "ConnectionState": "NotConnected",
                    "GUID": "stub_cellular1",
                    "Name": "cellular1",
                    "Type": "Cellular"
                  }], result);
      }));
  },
  function getVisibleNetworksWifi() {
    chrome.networkingPrivate.getVisibleNetworks(
      "WiFi",
      callbackPass(function(result) {
        assertEq([{
                    "ConnectionState": "Connected",
                    "GUID": "stub_wifi1",
                    "Name": "wifi1",
                    "Type": "WiFi",
                    "WiFi": {
                      "Security": "WEP-PSK",
                      "SignalStrength": 0
                    }
                  },
                  {
                    "ConnectionState": "NotConnected",
                    "GUID": "stub_wifi2",
                    "Name": "wifi2_PSK",
                    "Type": "WiFi",
                    "WiFi": {
                      "Security": "WPA-PSK",
                      "SignalStrength": 80
                    }
                  }
                  ], result);
      }));
  },
  function requestNetworkScan() {
    // Connected or Connecting networks should be listed first, sorted by type.
    var expected = ["stub_ethernet",
                    "stub_wifi1",
                    "stub_vpn1",
                    "stub_wifi2",
                    "stub_cellular1"];
    var done = chrome.test.callbackAdded();
    var listener = new privateHelpers.listListener(expected, done);
    chrome.networkingPrivate.onNetworkListChanged.addListener(
      listener.listenForChanges);
    chrome.networkingPrivate.requestNetworkScan();
  },
  function getProperties() {
    chrome.networkingPrivate.getProperties(
      "stub_wifi2",
      callbackPass(function(result) {
        assertEq({
                   "ConnectionState": "NotConnected",
                   "GUID": "stub_wifi2",
                   "Name": "wifi2_PSK",
                   "Type": "WiFi",
                   "WiFi": {
                     "Frequency": 5000,
                     "FrequencyList": [2400, 5000],
                     "SSID": "wifi2_PSK",
                     "Security": "WPA-PSK",
                     "SignalStrength": 80
                   }
                 }, result);
      }));
  },
  function getManagedProperties() {
    chrome.networkingPrivate.getManagedProperties(
      "stub_wifi2",
      callbackPass(function(result) {
        assertEq({
                   "ConnectionState": {
                     "Active": "NotConnected",
                     "Effective": "Unmanaged"
                   },
                   "GUID": "stub_wifi2",
                   "Name": {
                     "Active": "wifi2_PSK",
                     "Effective": "UserPolicy",
                     "UserPolicy": "My WiFi Network"
                   },
                   "Type": {
                     "Active": "WiFi",
                     "Effective": "UserPolicy",
                     "UserPolicy": "WiFi"
                   },
                   "WiFi": {
                     "AutoConnect": {
                       "Active": false,
                       "UserEditable": true
                     },
                     "Frequency" : {
                       "Active": 5000,
                       "Effective": "Unmanaged"
                     },
                     "FrequencyList" : {
                       "Active": [2400, 5000],
                       "Effective": "Unmanaged"
                     },
                     "Passphrase": {
                       "Effective": "UserSetting",
                       "UserEditable": true,
                       "UserSetting": "FAKE_CREDENTIAL_VPaJDV9x"
                     },
                     "SSID": {
                       "Active": "wifi2_PSK",
                       "Effective": "UserPolicy",
                       "UserPolicy": "wifi2_PSK"
                     },
                     "Security": {
                       "Active": "WPA-PSK",
                       "Effective": "UserPolicy",
                       "UserPolicy": "WPA-PSK"
                     },
                     "SignalStrength": {
                       "Active": 80,
                       "Effective": "Unmanaged"
                     }
                   }
                 }, result);
      }));
  },
  function setProperties() {
    var done = chrome.test.callbackAdded();
    chrome.networkingPrivate.getProperties(
      "stub_wifi2",
      callbackPass(function(result) {
        result.WiFi.Security = "WEP-PSK";
        chrome.networkingPrivate.setProperties("stub_wifi2", result,
          callbackPass(function() {
            chrome.networkingPrivate.getProperties(
              "stub_wifi2",
              callbackPass(function(result) {
                assertEq("WEP-PSK", result.WiFi.Security);
                done();
              }));
          }));
      }));
  },
  function getState() {
    chrome.networkingPrivate.getState(
      "stub_wifi2",
      callbackPass(function(result) {
        assertEq({
          "ConnectionState": "NotConnected",
          "GUID": "stub_wifi2",
          "Name": "wifi2_PSK",
          "Type": "WiFi",
          "WiFi": {
            "Security": "WPA-PSK",
            "SignalStrength": 80
          }
        }, result);
      }));
  },
  function getStateNonExistent() {
    chrome.networkingPrivate.getState(
      'non_existent',
      callbackFail('Error.InvalidParameter'));
  },
  function onNetworksChangedEventConnect() {
    var network = "stub_wifi2";
    var done = chrome.test.callbackAdded();
    var expectedStates = ["Connected"];
    var listener =
        new privateHelpers.watchForStateChanges(network, expectedStates, done);
    chrome.networkingPrivate.startConnect(network, callbackPass());
  },
  function onNetworksChangedEventDisconnect() {
    var network = "stub_wifi1";
    var done = chrome.test.callbackAdded();
    var expectedStates = ["NotConnected"];
    var listener =
        new privateHelpers.watchForStateChanges(network, expectedStates, done);
    chrome.networkingPrivate.startDisconnect(network, callbackPass());
  },
  function onNetworkListChangedEvent() {
    // Connecting to wifi2 should set wifi1 to offline. Connected or Connecting
    // networks should be listed first, sorted by type.
    var expected = ["stub_ethernet",
                    "stub_wifi2",
                    "stub_vpn1",
                    "stub_wifi1",
                    "stub_cellular1"];
    var done = chrome.test.callbackAdded();
    var listener = new privateHelpers.listListener(expected, done);
    chrome.networkingPrivate.onNetworkListChanged.addListener(
      listener.listenForChanges);
    var network = "stub_wifi2";
    chrome.networkingPrivate.startConnect(network, callbackPass());
  },
  function verifyDestination() {
    chrome.networkingPrivate.verifyDestination(
      verificationProperties,
      callbackPass(function(isValid) {
        assertTrue(isValid);
      }));
  },
  function verifyAndEncryptCredentials() {
    chrome.networkingPrivate.verifyAndEncryptCredentials(
      verificationProperties,
      "guid",
      callbackPass(function(result) {
        assertEq("encrypted_credentials", result);
      }));
  },
  function verifyAndEncryptData() {
    chrome.networkingPrivate.verifyAndEncryptData(
      verificationProperties,
      "data",
      callbackPass(function(result) {
        assertEq("encrypted_data", result);
      }));
  },
  function setWifiTDLSEnabledState() {
    chrome.networkingPrivate.setWifiTDLSEnabledState(
      "aa:bb:cc:dd:ee:ff",
      true,
      callbackPass(function(result) {
        assertEq("Connected", result);
      }));
  },
  function getWifiTDLSStatus() {
    chrome.networkingPrivate.getWifiTDLSStatus(
      "aa:bb:cc:dd:ee:ff",
      callbackPass(function(result) {
        assertEq("Connected", result);
      }));
  },
  function getCaptivePortalStatus() {
    var networks = [['stub_ethernet', 'Online'],
                    ['stub_wifi1', 'Offline'],
                    ['stub_wifi2', 'Portal'],
                    ['stub_cellular1', 'ProxyAuthRequired'],
                    ['stub_vpn1', 'Unknown']];
    networks.forEach(function(network) {
      var servicePath = network[0];
      var expectedStatus = network[1];
      chrome.networkingPrivate.getCaptivePortalStatus(
        servicePath,
        callbackPass(function(status) {
          assertEq(expectedStatus, status);
        }));
    });
  }
];

var testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));
