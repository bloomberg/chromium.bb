// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test
// Run with browser_tests
// --gtest_filter=ExtensionPreferenceApiTest.DataReductionProxy

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
    dataReductionProxy.dataReductionDailyContentLength.get({},
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(
              {
                'value': [],
                'levelOfControl': 'controllable_by_this_extension'
              },
              result);
    }));
    dataReductionProxy.dataReductionDailyReceivedLength.get({},
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(
              {
                'value': [],
                'levelOfControl': 'controllable_by_this_extension'
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
    dataReductionProxy.dataReductionDailyContentLength.onChange.addListener(
        confirmDailyContentLength);
    dataReductionProxy.dataReductionDailyReceivedLength.onChange.addListener(
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
      dataReductionProxy.dataReductionDailyReceivedLength.get({},
          chrome.test.callbackPass(function(result) {
            chrome.test.assertEq(
                {
                  'value': expectedDailyLengths ,
                  'levelOfControl': 'controllable_by_this_extension'
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
      dataReductionProxy.dataReductionDailyContentLength.get({},
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(
              {
                'value': expectedDailyLengths ,
                'levelOfControl': 'controllable_by_this_extension'
              },
              result);
          dataReductionProxy.dataReductionDailyContentLength.onChange.
              removeListener(confirmDailyContentLength);
      }));
      privatePreferences.dataReductionUpdateDailyLengths.get({},
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(
              {
                'value': false
              },
              result);
          dataReductionProxy.dataReductionDailyReceivedLength.onChange.
              removeListener(confirmRecievedLength);
      }));
    }
  },
  function clearDataSavings() {
    dataReductionProxy.spdyProxyEnabled.set({ 'value': true });

    dataReductionProxy.clearDataSavings(function() {
        // Confirm that data is cleared
        dataReductionProxy.dataReductionDailyContentLength.get({},
          chrome.test.callbackPass(function(result) {
            chrome.test.assertEq(
              {
                'value': [],
                'levelOfControl': 'controllable_by_this_extension'
              },
              result);
          }));
        dataReductionProxy.dataReductionDailyReceivedLength.get({},
          chrome.test.callbackPass(function(result) {
            chrome.test.assertEq(
              {
                'value': [],
                'levelOfControl': 'controllable_by_this_extension'
              },
              result);
          }));
    });
  },
  function dataUsageReporting() {
    dataReductionProxy.dataUsageReportingEnabled.set({ 'value': true });

    // Data usage reporting takes some time to initialize before a call to
    // |getDataUsage| is successful. If |getDataUsage| gives us an empty array,
    // we retry after some delay. Test will report failure if the expected
    // data usage is not returned after 20 retries.
    var verifyDataUsage = function(numRetries) {
      chrome.test.assertTrue(numRetries != 0);

      setTimeout(chrome.test.callbackPass(function() {
        dataReductionProxy.getDataUsage(chrome.test.callbackPass(
          function(data_usage) {
            chrome.test.assertTrue('data_usage_buckets' in data_usage);
            if (data_usage['data_usage_buckets'].length == 0) {
              verifyDataUsage(numRetries - 1);
            } else {
              chrome.test.assertEq(5760,
                                   data_usage['data_usage_buckets'].length);
            }
        }));
      }), 1000);
    };

    verifyDataUsage(20);
  }
]);
