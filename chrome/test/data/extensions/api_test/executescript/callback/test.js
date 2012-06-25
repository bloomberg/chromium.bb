// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var relativePath =
    '/files/extensions/api_test/executescript/callback/test.html';
var testUrl = 'http://b.com:PORT' + relativePath;

chrome.test.getConfig(function(config) {
  testUrl = testUrl.replace(/PORT/, config.testServer.port);
  chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
    if (changeInfo.status != 'complete')
      return;
    chrome.tabs.onUpdated.removeListener(arguments.callee);
    chrome.test.runTests([

      function executeCallbackIntShouldSucceed() {
        var scriptDetails = {code: '3'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq(3, scriptVal[0]);
          }));
        });
      },

      function executeCallbackDoubleShouldSucceed() {
        var scriptDetails = {code: '1.4'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq(1.4, scriptVal[0]);
          }));
        });
      },

      function executeCallbackStringShouldSucceed() {
        var scriptDetails = {code: '"foobar"'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq('foobar', scriptVal[0]);
          }));
        });
      },

      function executeCallbackTrueShouldSucceed() {
        var scriptDetails = {code: 'true'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq(true, scriptVal[0]);
          }));
        });
      },

      function executeCallbackFalseShouldSucceed() {
        var scriptDetails = {code: 'false'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq(false, scriptVal[0]);
          }));
        });
      },

      function executeCallbackNullShouldSucceed() {
        var scriptDetails = {code: 'null'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq(null, scriptVal[0]);
          }));
        });
      },

      function executeCallbackArrayShouldSucceed() {
        var scriptDetails = {code: '[1, "5", false, null]'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq([1, "5", false, null], scriptVal[0]);
          }));
        });
      },

      function executeCallbackObjShouldSucceed() {
        var scriptDetails = {code: 'var obj = {"id": "foo", "bar": 9}; obj'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq({"id": "foo", "bar": 9}, scriptVal[0]);
          }));
        });
      }
    ]);
  });
  chrome.tabs.create({ url: testUrl });
});
