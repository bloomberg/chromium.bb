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
  connectListener: function(network, done) {
    var self = this;
    var collectProperties = function(transition, properties) {
      var finishTest = function() {
        chrome.networkingPrivate.onNetworksChanged.removeListener(
          self.watchForConnect);
        done();
      };
      var startDisconnect = function() {
        chrome.networkingPrivate.startDisconnect(
          network,
          callbackPass(function() {}));
      };
      var startConnect = function() {
        chrome.networkingPrivate.startConnect(
          network,
          callbackPass(function() {}));
      };
      if (properties.ConnectionState == "NotConnected") {
        if (transition == "connect")
          finishTest();
        else
          startConnect();
      }
      if (properties.ConnectionState == "Connected") {
        if (transition == "connect")
          startDisconnect();
        else
          finishTest();
      }
    };
    this.watchForConnect = function(changes) {
      assertEq([network], changes);
      chrome.networkingPrivate.getProperties(
        network, collectProperties.bind(undefined, "connect"));
    };
    this.watchForDisconnect = function(changes) {
      assertEq([network], changes);
      chrome.networkingPrivate.getProperties(
        network, collectProperties.bind(undefined, "disconnect"));
    };
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

/////////////////////////////////////////////////////
// Tests
var availableTests = [
  function startConnect() {
    chrome.networkingPrivate.startConnect("stub_wifi2",
      callbackPass(function() {}));
  },
  function startDisconnect() {
    chrome.networkingPrivate.startDisconnect("stub_wifi2",
      callbackPass(function() {}));
  },
  function startConnectNonexistent() {
    // Make sure we get an error when we try to connect to a nonexistent
    // network.
    chrome.networkingPrivate.startConnect(
      "nonexistent_path",
      callbackFail("Error.InvalidService", function() {}));
  },
  function getVisibleNetworks() {
    chrome.networkingPrivate.getVisibleNetworks(
      "All",
      callbackPass(function(result) {
        assertTrue(!!result);
        assertEq(4, result.length);
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
                      "SSID": "stub_wifi1",
                    }
                  },
                  {
                    "ConnectionState": "NotConnected",
                    "GUID": "stub_wifi2",
                    "Name": "wifi2_PSK",
                    "Type": "WiFi",
                    "WiFi": {
                      "SSID": "stub_wifi2",
                      "Security": "WPA-PSK",
                      "SignalStrength": 80
                    }
                  },
                  {
                    "ConnectionState": "NotConnected",
                    "GUID": "stub_cellular1",
                    "Name": "cellular1",
                    "Type": "Cellular"
                  }
                  ], result);
      }));
  },
  function getVisibleNetworksWifi() {
    chrome.networkingPrivate.getVisibleNetworks(
      "WiFi",
      callbackPass(function(result) {
        assertTrue(!!result);
        assertEq(2, result.length);
        assertEq([{
                    "ConnectionState": "Connected",
                    "GUID": "stub_wifi1",
                    "Name": "wifi1",
                    "Type": "WiFi",
                    "WiFi": {
                      "SSID": "stub_wifi1"
                    }
                  },
                  {
                    "ConnectionState": "NotConnected",
                    "GUID": "stub_wifi2",
                    "Name": "wifi2_PSK",
                    "Type": "WiFi",
                    "WiFi": {
                      "SSID": "stub_wifi2",
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
        assertEq("wifi2_PSK", result.Name);
        assertEq("NotConnected", result.ConnectionState);
        assertEq("WiFi", result.Type);
      }));
  },
  function onNetworksChangedEvent() {
    var network = "stub_wifi2";
    var done = chrome.test.callbackAdded();
    var listener = new privateHelpers.connectListener(network, done);
    chrome.networkingPrivate.onNetworksChanged.addListener(
      listener.watchForConnect);
    chrome.networkingPrivate.startConnect(network,
      callbackPass(function() {}));
  },
  function onNetworkListChangedEvent() {
    var network = "stub_wifi2";
    var expected = ["stub_wifi2",
                    "stub_ethernet",
                    "stub_wifi1",
                    "stub_cellular1"];
    var done = chrome.test.callbackAdded();
    var listener = new privateHelpers.listListener(network, expected, done);
    chrome.networkingPrivate.onNetworkListChanged.addListener(
      listener.listenForChanges);
    chrome.networkingPrivate.startConnect(network,
      callbackPass(function() {}));
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