// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;
var assertTrue = chrome.test.assertTrue;
var assertEq = chrome.test.assertEq;

// Test properties for the verification API.
var verificationProperties = {
  "certificate": "certificate",
  "publicKey": "public_key",
  "nonce": "nonce",
  "signedData": "signed_data",
  "deviceSerial": "device_serial"
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
          self.watchForConnect);
        done();
      };
      var expectedState = expectedStates.pop();
      assertEq(expectedState, properties.ConnectionState);
      if (expectedStates.length == 0)
        finishTest();
    };
    this.onNetworkChange = function(changes) {
      assertEq([network], changes);
      chrome.networkingPrivate.getProperties(network,
                                             collectProperties.bind(undefined));
    };
    chrome.networkingPrivate.onNetworksChanged.addListener(
      this.onNetworkChange);
  },
  listListener: function(network, expected, done) {
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
    chrome.networkingPrivate.startDisconnect("stub_wifi2", callbackPass());
  },
  function startConnectNonexistent() {
    chrome.networkingPrivate.startConnect(
      "nonexistent_path",
      callbackFail("Error.InvalidService"));
  },
  function startDisconnectNonexistent() {
    chrome.networkingPrivate.startDisconnect(
      "nonexistent_path",
      callbackFail("Error.InvalidService"));
  },
  function startGetPropertiesNonexistent() {
    chrome.networkingPrivate.getProperties(
      "nonexistent_path",
      callbackFail("Error.DBusFailed"));
  },
  function getVisibleNetworks() {
    chrome.networkingPrivate.getVisibleNetworks(
      "All",
      callbackPass(function(result) {
        assertTrue(!!result);
        assertEq([{
                    "ConnectionState": "Connected",
                    "GUID": "stub_ethernet",
                    "Name": "eth0",
                    "Type": "Ethernet"
                  },
                  {
                    "ConnectionState": "Connected",
                    "GUID": "stub_wifi1",
                    "Name": "wifi1",
                    "Type": "WiFi",
                    "WiFi": {
                      "AutoConnect": false,
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
                      "AutoConnect": false,
                      "Security": "WPA-PSK",
                      "SignalStrength": 80
                    }
                  },
                  {
                    "Cellular": {
                      "ActivationState": "not-activated",
                      "NetworkTechnology": "GSM",
                      "RoamingState": "home"
                    },
                    "ConnectionState": "NotConnected",
                    "GUID": "stub_cellular1",
                    "Name": "cellular1",
                    "Type": "Cellular"
                  },
                  {
                    "ConnectionState": "Connected",
                    "GUID": "stub_vpn1",
                    "Name": "vpn1",
                    "Type": "VPN",
                    "VPN": {
                      "AutoConnect": false
                    }
                  }], result);
      }));
  },
  function getVisibleNetworksWifi() {
    chrome.networkingPrivate.getVisibleNetworks(
      "WiFi",
      callbackPass(function(result) {
        assertTrue(!!result);
        assertEq([{
                    "ConnectionState": "Connected",
                    "GUID": "stub_wifi1",
                    "Name": "wifi1",
                    "Type": "WiFi",
                    "WiFi": {
                      "AutoConnect": false,
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
                      "AutoConnect": false,
                      "Security": "WPA-PSK",
                      "SignalStrength": 80
                    }
                  }
                  ], result);
      }));
  },
  function getProperties() {
    chrome.networkingPrivate.getProperties(
      "stub_wifi2",
      callbackPass(function(result) {
        assertTrue(!!result);
        assertEq({
                   "ConnectionState": "NotConnected",
                   "GUID": "stub_wifi2",
                   "Name": "wifi2_PSK",
                   "Type": "WiFi",
                   "WiFi": {
                     "SSID": "stub_wifi2",
                     "Security": "WPA-PSK",
                     "SignalStrength": 80
                   }
                 }, result);
      }));
  },
  function setProperties() {
    var done = chrome.test.callbackAdded();
    chrome.networkingPrivate.getProperties(
      "stub_wifi2",
      function(result) {
        result.WiFi.Security = "WEP-PSK";
        chrome.networkingPrivate.setProperties("stub_wifi2", result,
          function() {
            chrome.networkingPrivate.getProperties(
              "stub_wifi2",
              function(result) {
                assertEq("WEP-PSK", result.WiFi.Security);
                done();
              });
          });
      });
  },
  function getState() {
    chrome.networkingPrivate.getState(
      "stub_wifi2",
      callbackPass(function(result) {
        assertEq({
          "ConnectionState": "NotConnected",
          "GUID": "",
          "Name": "wifi2_PSK",
          "Type": "WiFi",
          "WiFi": {
            "AutoConnect": false,
            "Security": "WPA-PSK",
            "SignalStrength": 80
          }
        }, result);
      }));
  },
  function onNetworksChangedEventConnect() {
    var network = "stub_wifi2";
    var done = chrome.test.callbackAdded();
    var expectedStates = ["Connected", "Connecting"];
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
    var network = "stub_wifi2";
    var expected = ["stub_wifi2",
                    "stub_ethernet",
                    "stub_wifi1",
                    "stub_cellular1",
                    "stub_vpn1"];
    var done = chrome.test.callbackAdded();
    var listener = new privateHelpers.listListener(network, expected, done);
    chrome.networkingPrivate.onNetworkListChanged.addListener(
      listener.listenForChanges);
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
  }
];

var testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));
