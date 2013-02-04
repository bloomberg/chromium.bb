// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;
var assertTrue = chrome.test.assertTrue;
var assertEq = chrome.test.assertEq;

chrome.test.runTests([
  function getVisibleNetworks() {
    chrome.networkingPrivate.getVisibleNetworks(
        "All",
        callbackPass(function(result) {
          assertTrue(!!result);
          assertEq(4, result.length);
          assertEq([{ "Name": "eth0",
                      "GUID": "stub_ethernet",
                      "ConnectionState": "Connected",
                      "Type": "Ethernet"
                    },
                    { "Name": "wifi1",
                      "GUID": "stub_wifi1",
                      "ConnectionState": "Connected",
                      "Type": "WiFi",
                      "WiFi": {
                        "SSID": "stub_wifi1",
                        "Type": "WiFi"
                      }
                    },
                    { "Name": "wifi2_PSK",
                      "GUID": "stub_wifi2",
                      "ConnectionState": "NotConnected",
                      "Type": "WiFi",
                      "WiFi": {
                        "SSID": "stub_wifi2",
                        "Type": "WiFi"
                      }
                    },
                    { "Name": "cellular1",
                      "GUID": "stub_cellular1",
                      "ConnectionState": "NotConnected",
                      "Type": "Cellular"
                    }], result);
        }));
  },
  function getVisibleNetworksWifi() {
    chrome.networkingPrivate.getVisibleNetworks(
      "WiFi",
      callbackPass(function(result) {
        assertTrue(!!result);
        assertEq(2, result.length);
        assertEq([{ "Name": "wifi1",
                    "GUID": "stub_wifi1",
                    "ConnectionState": "Connected",
                    "Type": "WiFi",
                    "WiFi": {
                      "SSID": "stub_wifi1",
                      "Type":"WiFi"
                    }
                  },
                  { "Name": "wifi2_PSK",
                    "GUID": "stub_wifi2",
                    "ConnectionState": "NotConnected",
                    "Type": "WiFi",
                    "WiFi": {
                      "SSID": "stub_wifi2",
                      "Type": "WiFi"
                    }
                  }], result);
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
  }
]);
