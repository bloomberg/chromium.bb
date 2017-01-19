// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (!chrome || !chrome.test)
  throw new Error('chrome.test is undefined');

var portNumber;

// This is a good end-to-end test for two reasons. The first is obvious - it
// tests a simple API and makes sure it behaves as expected, as well as testing
// that other APIs are unavailable.
// The second is that chrome.test is itself an extension API, and a rather
// complex one. It requires both traditional bindings (renderer parses args,
// passes info to browser process, browser process does work and responds, re-
// enters JS) and custom JS bindings (in order to have our runTests, assert*
// methods, etc). If any of these stages failed, the test itself would also
// fail.
var tests = [
  function idleApi() {
    chrome.test.assertTrue(!!chrome.idle);
    chrome.test.assertTrue(!!chrome.idle.IdleState);
    chrome.test.assertTrue(!!chrome.idle.IdleState.IDLE);
    chrome.test.assertTrue(!!chrome.idle.IdleState.ACTIVE);
    chrome.test.assertTrue(!!chrome.idle.queryState);
    chrome.idle.queryState(1000, function(state) {
      // Depending on the machine, this could come back as either idle or
      // active. However, all we're curious about is the bindings themselves
      // (not the API implementation), so as long as it's a possible response,
      // it's a success for our purposes.
      chrome.test.assertTrue(state == chrome.idle.IdleState.IDLE ||
                             state == chrome.idle.IdleState.ACTIVE);
      chrome.test.succeed();
    });
  },
  function nonexistentApi() {
    chrome.test.assertFalse(!!chrome.nonexistent);
    chrome.test.succeed();
  },
  function disallowedApi() {
    chrome.test.assertFalse(!!chrome.power);
    chrome.test.succeed();
  },
  function events() {
    var createdEvent = new Promise((resolve, reject) => {
      chrome.tabs.onCreated.addListener(tab => {
        resolve(tab.id);
      });
    });
    var createdCallback = new Promise((resolve, reject) => {
      chrome.tabs.create({url: 'http://example.com'}, tab => {
        resolve(tab.id);
      });
    });
    Promise.all([createdEvent, createdCallback]).then(res => {
      chrome.test.assertEq(2, res.length);
      chrome.test.assertEq(res[0], res[1]);
      chrome.test.succeed();
    });
  },
  function testMessaging() {
    var tabId;
    var createPort = function() {
      chrome.test.assertTrue(!!tabId);
      var port = chrome.tabs.connect(tabId);
      chrome.test.assertTrue(!!port, 'Port does not exist');
      port.onMessage.addListener(message => {
        chrome.test.assertEq('content script', message);
        port.disconnect();
        chrome.test.succeed();
      });
      port.postMessage('background page');
    };

    chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
      chrome.test.assertEq('startFlow', message);
      createPort();
      sendResponse('started');
    });
    var url = 'http://localhost:' + portNumber +
              '/native_bindings/messaging_test.html';
    chrome.tabs.create({url: url}, function(tab) {
      chrome.test.assertNoLastError();
      chrome.test.assertTrue(!!tab);
      chrome.test.assertTrue(!!tab.id && tab.id >= 0);
      tabId = tab.id;
    });
  },
];

chrome.test.getConfig(config => {
  chrome.test.assertTrue(!!config, 'config does not exist');
  chrome.test.assertTrue(!!config.testServer, 'testServer does not exist');
  portNumber = config.testServer.port;
  chrome.test.runTests(tests);
});
