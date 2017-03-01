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
              '/native_bindings/extension/messaging_test.html';
    chrome.tabs.create({url: url}, function(tab) {
      chrome.test.assertNoLastError();
      chrome.test.assertTrue(!!tab);
      chrome.test.assertTrue(!!tab.id && tab.id >= 0);
      tabId = tab.id;
    });
  },
  function castStreaming() {
    // chrome.cast.streaming APIs are the only ones that are triply-prefixed.
    chrome.test.assertTrue(!!chrome.cast);
    chrome.test.assertTrue(!!chrome.cast.streaming);
    chrome.test.assertTrue(!!chrome.cast.streaming.udpTransport);
    chrome.test.assertTrue(!!chrome.cast.streaming.udpTransport.setOptions);
    chrome.test.succeed();
  },
  function injectScript() {
    var url =
        'http://example.com:' + portNumber + '/native_bindings/simple.html';
    // Create a tab, and inject code in it to change its title.
    // chrome.tabs.executeScript relies on external type references
    // (extensionTypes.InjectDetails), so this exercises that flow as well.
    chrome.tabs.create({url: url}, function(tab) {
      chrome.test.assertTrue(!!tab, 'tab');
      // Snag this opportunity to test bindings properties.
      chrome.test.assertTrue(!!chrome.tabs.TAB_ID_NONE);
      chrome.test.assertTrue(tab.id != chrome.tabs.TAB_ID_NONE);
      chrome.test.assertEq(new URL(url).host, new URL(tab.url).host);
      var code = 'document.title = "new title";';
      chrome.tabs.executeScript(tab.id, {code: code}, function(results) {
        chrome.test.assertTrue(!!results, 'results');
        chrome.test.assertEq(1, results.length);
        chrome.test.assertEq('new title', results[0]);
        chrome.tabs.get(tab.id, tab => {
          chrome.test.assertEq('new title', tab.title);
          chrome.test.succeed();
        });
      });
    });
  },
  function testLastError() {
    chrome.runtime.setUninstallURL('chrome://newtab', function() {
      chrome.test.assertLastError('Invalid URL: "chrome://newtab".');
      chrome.test.succeed();
    });
  },
  function testStorage() {
    // Check API existence; StorageArea functions.
    chrome.test.assertTrue(!!chrome.storage);
    chrome.test.assertTrue(!!chrome.storage.local, 'no local');
    chrome.test.assertTrue(!!chrome.storage.local.set, 'no set');
    chrome.test.assertTrue(!!chrome.storage.local.get, 'no get');
    // Check some properties.
    chrome.test.assertTrue(!!chrome.storage.local.QUOTA_BYTES,
                           'local quota bytes');
    chrome.test.assertFalse(!!chrome.storage.local.MAX_ITEMS,
                            'local max items');
    chrome.test.assertTrue(!!chrome.storage.sync, 'sync');
    chrome.test.assertTrue(!!chrome.storage.sync.QUOTA_BYTES,
                           'sync quota bytes');
    chrome.test.assertTrue(!!chrome.storage.sync.MAX_ITEMS,
                           'sync max items');
    chrome.test.assertTrue(!!chrome.storage.managed, 'managed');
    chrome.test.assertFalse(!!chrome.storage.managed.QUOTA_BYTES,
                            'managed quota bytes');
    chrome.storage.local.set({foo: 'bar'}, () => {
      chrome.storage.local.get('foo', (results) => {
        chrome.test.assertTrue(!!results, 'no results');
        chrome.test.assertTrue(!!results.foo, 'no foo');
        chrome.test.assertEq('bar', results.foo);
        chrome.test.succeed();
      });
    });
  },
  function testBrowserActionWithCustomSendRequest() {
    // browserAction.setIcon uses a custom hook that calls sendRequest().
    chrome.browserAction.setIcon({path: 'icon.png'}, chrome.test.succeed);
  },
  function testChromeSetting() {
    chrome.test.assertTrue(!!chrome.privacy, 'privacy');
    chrome.test.assertTrue(!!chrome.privacy.websites, 'websites');
    var cookiePolicy = chrome.privacy.websites.thirdPartyCookiesAllowed;
    chrome.test.assertTrue(!!cookiePolicy, 'cookie policy');
    chrome.test.assertTrue(!!cookiePolicy.get, 'get');
    chrome.test.assertTrue(!!cookiePolicy.set, 'set');
    chrome.test.assertTrue(!!cookiePolicy.clear, 'clear');
    chrome.test.assertTrue(!!cookiePolicy.onChange, 'onchange');

    // The JSON spec for ChromeSettings is weird, because it claims it allows
    // any type for <val> in ChromeSetting.set({value: <val>}), but this is just
    // a hack in our schema generation because we curry in the different types
    // of restrictions. Trying to pass in the wrong type for value should fail
    // (synchronously).
    var caught = false;
    try {
      cookiePolicy.set({value: 'not a bool'});
    } catch (e) {
      caught = true;
    }
    chrome.test.assertTrue(caught, 'caught');

    var listenerPromise = new Promise((resolve, reject) => {
      cookiePolicy.onChange.addListener(function listener(details) {
        chrome.test.assertTrue(!!details, 'listener details');
        chrome.test.assertEq(false, details.value);
        cookiePolicy.onChange.removeListener(listener);
        resolve();
      });
    });

    var methodPromise = new Promise((resolve, reject) => {
      cookiePolicy.get({}, (details) => {
        chrome.test.assertTrue(!!details, 'get details');
        chrome.test.assertTrue(details.value, 'details value true');
        cookiePolicy.set({value: false}, () => {
          cookiePolicy.get({}, (details) => {
            chrome.test.assertTrue(!!details, 'get details');
            chrome.test.assertFalse(details.value, 'details value false');
            resolve();
          });
        });
      });
    });

    Promise.all([listenerPromise, methodPromise]).then(() => {
      chrome.test.succeed();
    });
  },
];

chrome.test.getConfig(config => {
  chrome.test.assertTrue(!!config, 'config does not exist');
  chrome.test.assertTrue(!!config.testServer, 'testServer does not exist');
  portNumber = config.testServer.port;
  chrome.test.runTests(tests);
});
