// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.tests = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';

embedder.setUp = function(config) {
  embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
  embedder.guestURL = embedder.baseGuestURL +
      '/files/extensions/platform_apps/web_view/screen_coordinates' +
      '/guest.html';
  chrome.test.log('Guest url is: ' + embedder.guestURL);
};

/** @private */
embedder.setUpGuest_ = function() {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"' +
      ' src="' + embedder.guestURL + '"' +
      '></webview>';
  var webview = document.querySelector('webview');
  if (!webview) {
    chrome.test.fail('No <webview> element created');
  }
  return webview;
};

/** @private */
embedder.setUpLoadStop_ = function(webview, testName) {
  var onWebViewLoadStop = function(e) {
    // Send post message to <webview> when it's ready to receive them.
    webview.contentWindow.postMessage(
        JSON.stringify(['' + testName, 'get-screen-info']), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
};

/**
 * @private
 * @param {Number} mn Expected range's min value.
 * @param {Number} mx Expected range's max value.
 * @param {Number} a Actual value.
 */
embedder.assertCorrectCoordinateValue_ = function(mn, mx, a, msg) {
  chrome.test.assertTrue(
      a >= mn && a <= mx,
      'Actual value [' + a + '] is not within interval ' +
      '[' + mn + ', ' + mx + ']');
};

/** @private */
embedder.registerAndWaitForPostMessage_ = function(
    webview, expectedData) {
  var testName = expectedData[0];
  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == '' + testName) {
      embedder.assertCorrectCoordinateValue_(
          window.screenX, window.screenX + window.innerWidth,
          data[1].screenX, 'screenX');
      embedder.assertCorrectCoordinateValue_(
          window.screenY, window.screenY + window.innerHeight,
          data[1].screenY, 'screenY');
      embedder.assertCorrectCoordinateValue_(
          window.screenLeft, window.screenLeft + window.innerWidth,
          data[1].screenLeft, 'screenLeft');
      embedder.assertCorrectCoordinateValue_(
          window.screenTop, window.screenTop + window.innerHeight,
          data[1].screenTop, 'screenTop');
      chrome.test.succeed();
    }
  };
  window.addEventListener('message', onPostMessageReceived);
};


onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp(config);
    chrome.test.runTests([
      function testScreenCoordinates() {
        var webview = embedder.setUpGuest_();
        embedder.setUpLoadStop_(webview, 'test1');
        embedder.registerAndWaitForPostMessage_(webview, ['test1']);
      }
    ]);
  });
};
