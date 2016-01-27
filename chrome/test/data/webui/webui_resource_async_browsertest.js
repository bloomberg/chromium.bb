// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Framework for running async JS tests for cr.js utility methods.
 */

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../';

/** @const {string} Name of the chrome.send() message to be used in tests. */
var CHROME_SEND_NAME = 'echoMessage';

/**
 * Test fixture for testing async methods of cr.js.
 * @constructor
 * @extends testing.Test
 */
function WebUIResourceAsyncTest() {}

WebUIResourceAsyncTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: DUMMY_URL,

  /** @override */
  isAsync: true,

  /** @override */
  runAccessibilityChecks: false,

  /** @override */
  extraLibraries: [
    ROOT_PATH + 'third_party/mocha/mocha.js',
    ROOT_PATH + 'chrome/test/data/webui/mocha_adapter.js',
    ROOT_PATH + 'ui/webui/resources/js/cr.js',
  ],
};

TEST_F('WebUIResourceAsyncTest', 'SendWithPromise', function() {
  /**
   * TODO(dpapad): Move this helper method in test_api.js.
   * @param {string} name chrome.send message name.
   * @return {!Promise} Fires when chrome.send is called with the given message
   *     name.
   */
  function whenChromeSendCalled(name) {
    return new Promise(function(resolve, reject) {
      registerMessageCallback(name, null, resolve);
    });
  }

  suite('cr.js', function() {
    setup(function() {
      // Simulate a WebUI handler that echoes back all parameters passed to it.
      whenChromeSendCalled(CHROME_SEND_NAME).then(function(args) {
        cr.webUIResponse.apply(null, args);
      });
    });

    test('sendWithPromise_ResponseObject', function() {
      var expectedResponse = {'foo': 'bar'};
      return cr.sendWithPromise(CHROME_SEND_NAME, expectedResponse).then(
          function(response) {
            assertEquals(
                JSON.stringify(expectedResponse), JSON.stringify(response));
          });
    });

    test('sendWithPromise_ResponseArray', function() {
      var expectedResponse = ['foo', 'bar'];
      return cr.sendWithPromise(CHROME_SEND_NAME, expectedResponse).then(
          function(response) {
            assertEquals(
                JSON.stringify(expectedResponse), JSON.stringify(response));
          });
    });

    test('sendWithPromise_ResponsePrimitive', function() {
      var expectedResponse = 1234;
      return cr.sendWithPromise(CHROME_SEND_NAME, expectedResponse).then(
          function(response) {
            assertEquals(expectedResponse, response);
          });
    });

    test('sendWithPromise_ResponseVoid', function() {
      return cr.sendWithPromise(CHROME_SEND_NAME).then(function(response) {
        assertEquals(undefined, response);
      });
    });
  });

  // Run all registered tests.
  mocha.run();
});
