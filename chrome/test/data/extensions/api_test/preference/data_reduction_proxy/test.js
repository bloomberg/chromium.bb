// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.DataReductionProxy

var dataReductionProxy = chrome.dataReductionProxy;
var privatePreferences = chrome.preferencesPrivate;
chrome.test.runTests([
  function getDrpPrefs() {
    dataReductionProxy.spdyProxyEnabled.get({}, chrome.test.callbackPass(
        function(result) {
          chrome.test.assertEq(
              {
                'value': false,
                'levelOfControl': 'controllable_by_this_extension'
              },
              result);
    }));
    privatePreferences.dataReductionDailyContentLength.get({},
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(
              {
                'value': []
              },
              result);
    }));
    privatePreferences.dataReductionDailyReceivedLength.get({},
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(
              {
                'value': []
              },
              result);
    }));
    privatePreferences.dataReductionUpdateDailyLengths.get({},
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(
              {
                'value': false
              },
              result);
    }));
  },
  function updateDailyLengths() {
    privatePreferences.dataReductionDailyContentLength.onChange.addListener(
        confirmDailyContentLength);
    privatePreferences.dataReductionDailyReceivedLength.onChange.addListener(
        confirmRecievedLength);

    // Trigger calls to confirmDailyContentLength.onChange and
    // dataReductionDailyReceivedLength.onChange listeners.
    dataReductionProxy.spdyProxyEnabled.set({ 'value': true });
    privatePreferences.dataReductionUpdateDailyLengths.set({'value': true});

    // Helper methods.
    var expectedDailyLengths = [];
    for (var i = 0; i < 60; i++) {
      expectedDailyLengths[i] = '0';
    }
    function confirmRecievedLength() {
      privatePreferences.dataReductionDailyReceivedLength.get({},
          chrome.test.callbackPass(function(result) {
            chrome.test.assertEq(
                {
                  'value': expectedDailyLengths
                },
                result);
      }));
      privatePreferences.dataReductionUpdateDailyLengths.get({},
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(
              {
                'value': false
              },
              result);
      }));
    }
    function confirmDailyContentLength() {
      privatePreferences.dataReductionDailyContentLength.get({},
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(
              {
                'value': expectedDailyLengths
              },
              result);
      }));
      privatePreferences.dataReductionUpdateDailyLengths.get({},
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(
              {
                'value': false
              },
              result);
      }));
    }
  }
]);
