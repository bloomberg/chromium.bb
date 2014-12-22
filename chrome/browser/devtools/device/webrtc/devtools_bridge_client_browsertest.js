// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "chrome/browser/devtools/device/webrtc/' +
    'devtools_bridge_client_browsertest.h"');

/**
 * Test fixture for DevToolsBridgeClientBrowserTest.
 * @constructor
 * @extends {testing.Test}
 */
function DevToolsBridgeClientBrowserTest() {}

var DEVICES_URL = "https://www.googleapis.com/clouddevices/v1/devices";

DevToolsBridgeClientBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  typedefCppFixture: 'DevToolsBridgeClientBrowserTest',

  /** @override */
  isAsync: true,

  /** @override */
  browsePreload: DUMMY_URL,

  setUp: function() {
    this.callbacksMock = mock(DevToolsBridgeClientBrowserTest.NativeCallbacks);
    window.callbacks = this.callbacksMock.proxy();
  },

  /**
   * Simulates user sign in. DevToolsBridgeClient creates
   * background_worker only in this case.
   */
  signIn: function() {
    chrome.send('signIn', []);
    return new Promise(function(resolve) {
      this.callbacksMock.expects(once()).workerLoaded().will(
          callFunction(resolve));
    }.bind(this));
  },

  /**
   * Creates GCD device definition which could be recognized as a
   * DevToolsBridge.
   *
   * @param {string} id GCD instance id.
   * @param {string} displayName Display name.
   */
  createInstanceDef: function(id, displayName) {
    return {
      'kind': 'clouddevices#device',
      'deviceKind': 'vendor',
      'id': id,
      'displayName': displayName,
      'commandDefs': {
        'base': {
          '_iceExchange': {'kind': 'clouddevices#commandDef'},
          '_renegotiate': {'kind': 'clouddevices#commandDef'},
          '_startSession': {'kind': 'clouddevices#commandDef'},
        }
      },
    };
  }
};

/**
 * Callbacks from native DevToolsBridgeClientBrowserTest.
 * @constructor
 */
DevToolsBridgeClientBrowserTest.NativeCallbacks = function() {}

DevToolsBridgeClientBrowserTest.NativeCallbacks.prototype = {
  workerLoaded: function() {},
  gcdApiRequest: function(id, body) {},
  browserListUpdated: function(count) {},
};

TEST_F('DevToolsBridgeClientBrowserTest', 'testSetUpOnMainThread', function() {
  testDone();
});

TEST_F('DevToolsBridgeClientBrowserTest', 'testSignIn', function() {
  this.signIn().then(testDone);
});

TEST_F('DevToolsBridgeClientBrowserTest', 'testQueryBrowsers', function() {
  this.signIn().then(function() {
    chrome.send('queryDevices');
  });
  var savedArgs = new SaveMockArguments();
  this.callbacksMock.expects(once()).gcdApiRequest(
      savedArgs.match(ANYTHING), DEVICES_URL, '').will(
          callFunctionWithSavedArgs(savedArgs, function(id) {
    var response = {
      'kind': 'clouddevices#devicesListResponse',
      'devices': [
        this.createInstanceDef(
            'ab911465-83c7-e335-ea64-cb656868cbe0', 'Test 1'),
        this.createInstanceDef(
            'ab911465-83c7-e335-ea64-cb656868cbe1', 'Test 2'),
        this.createInstanceDef(
            'ab911465-83c7-e335-ea64-cb656868cbe2', 'Test 3'),
      ],
    };
    chrome.send('gcdApiResponse', [id, response]);
  }.bind(this)));

  var browsersCount = 3;

  this.callbacksMock.expects(once()).browserListUpdated(browsersCount).will(
      callFunction(testDone));
});
