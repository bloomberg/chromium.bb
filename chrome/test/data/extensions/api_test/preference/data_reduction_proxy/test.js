// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.DataReductionProxy

var drp = chrome.dataReductionProxy;
chrome.test.runTests([
  function getDrpPrefs() {
    drp.spdyProxyEnabled.get({}, chrome.test.callbackPass(
        function(result) {
          chrome.test.assertEq(
              {
                'value': false,
                'levelOfControl': 'controllable_by_this_extension'
              },
              result);
    }));
    drp.dataReductionDailyContentLength.get({}, chrome.test.callbackPass(
        function(result) {
          chrome.test.assertEq(
              {
                'value': [],
                'levelOfControl': 'controllable_by_this_extension'
              },
              result);
    }));
    drp.dataReductionDailyReceivedLength.get({}, chrome.test.callbackPass(
        function(result) {
          chrome.test.assertEq(
              {
                'value': [],
                'levelOfControl': 'controllable_by_this_extension'
              },
              result);
    }));
    drp.dataReductionUpdateDailyLengths.get({}, chrome.test.callbackPass(
        function(result) {
          chrome.test.assertEq(
              {
                'value': false,
                'levelOfControl': 'controllable_by_this_extension'
              },
              result);
    }));
  },
  function updateDailyLengths() {
    drp.dataReductionDailyContentLength.onChange.addListener(
        confirmDailyContentLength);
    drp.dataReductionDailyReceivedLength.onChange.addListener(
        confirmRecievedLength);

    // Trigger calls to confirmDailyContentLength.onChange and
    // dataReductionDailyReceivedLength.onChange listeners.
    drp.spdyProxyEnabled.set({ 'value': true });
    drp.dataReductionUpdateDailyLengths.set({'value': true});

    // Helper methods.
    var expectedDailyLengths = [];
    for (var i = 0; i < 60; i++) {
      expectedDailyLengths[i] = '0';
    }
    function confirmRecievedLength() {
      drp.dataReductionDailyReceivedLength.get({},
          chrome.test.callbackPass(function(result) {
            chrome.test.assertEq(
                {
                  'value': expectedDailyLengths ,
                  'levelOfControl': 'controllable_by_this_extension'
                },
                result);
      }));
    }
    function confirmDailyContentLength() {
      drp.dataReductionDailyContentLength.get({},
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(
              {
                'value': expectedDailyLengths ,
                'levelOfControl': 'controllable_by_this_extension'
              },
              result);
      }));
    }
  }
]);
