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
      '/files/extensions/platform_apps/web_view/newwindow' +
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
embedder.setUpNewWindowRequest_ = function(webview, url, frameName) {
  var onWebViewLoadStop = function(e) {
    // Send post message to <webview> when it's ready to receive them.
    webview.contentWindow.postMessage(
        JSON.stringify(['open-window', '' + url, '' + frameName]), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
};

embedder.setUpFrameNameRequest_ = function(webview, testName) {
  var onWebViewLoadStop = function(e) {
    // Send post message to <webview> when it's ready to receive them.
    webview.contentWindow.postMessage(
        JSON.stringify(['get-frame-name', testName]), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
};

/** @private */
embedder.requestFrameName_ = function(webview, testName, expectedFrameName) {
  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == 'get-frame-name') {
      var name = data[1];
      if (testName != name)
        return;
      var frameName = data[2];
      chrome.test.assertEq(expectedFrameName, frameName);
      chrome.test.assertEq(expectedFrameName, webview.name);
      chrome.test.succeed();
    }
  };
  window.addEventListener('message', onPostMessageReceived);
};

/** @private */
embedder.assertCorrectEvent_ = function(e) {
  chrome.test.assertEq('newwindow', e.type);
  chrome.test.assertTrue(!!e.targetUrl);
  chrome.test.assertTrue(e.targetUrl.indexOf(embedder.baseGuestURL) == 0);
};

// Tests begin.

// The embedder has to initiate a post message so that the guest can get a
// reference to embedder to send the reply back.

var testNewWindow =
    function(testName, webViewName, guestName, expectedFrameName) {
  var webview = embedder.setUpGuest_();

  var onNewWindow = function(e) {
    chrome.test.log('Embedder notified on newwindow');
    embedder.assertCorrectEvent_(e);
    var width = e.initialWidth || 640;
    var height = e.initialHeight || 480;
    e.preventDefault();
    chrome.app.window.create('newwindow_embedder.html', {
      top: 0,
      left: 0,
      width: width,
      height: height,
    }, function(newwindow) {
      newwindow.contentWindow.onload = function(evt) {
        var w = newwindow.contentWindow;
        var newwebview = w.document.querySelector('webview');
        newwebview.name = webViewName;
        embedder.setUpFrameNameRequest_(newwebview, testName);
        embedder.requestFrameName_(newwebview, testName, expectedFrameName);
        try {
          e.window.attach(newwebview);
        } catch (e) {
          chrome.test.fail();
        }
      }
    });
  };
  webview.addEventListener('newwindow', onNewWindow);

  // Load a new window with the given name.
  embedder.setUpNewWindowRequest_(webview, 'guest.html', guestName);
};

// Loads a guest which requests a new window.
embedder.tests.testNewWindowNameTakesPrecedence =
    function testNewWindowNameTakesPrecedence() {
  var webViewName = 'foo';
  var guestName = 'bar';
  var expectedName = guestName;
  testNewWindow('testNewWindowNameTakesPrecedence',
                webViewName, guestName, expectedName);
};

embedder.tests.testWebViewNameTakesPrecedence =
    function testWebViewNameTakesPrecedence() {
  var webViewName = 'foo';
  var guestName = '';
  var expectedName = webViewName;
  testNewWindow('testWebViewNameTakesPrecedence',
                webViewName, guestName, expectedName);
};

embedder.tests.testNoName = function testNoName() {
  var webViewName = '';
  var guestName = '';
  var expectedName = '';
  testNewWindow('testNoName', webViewName, guestName, expectedName);
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp(config);
    chrome.test.runTests([
      embedder.tests.testNewWindowNameTakesPrecedence,
      embedder.tests.testWebViewNameTakesPrecedence,
      embedder.tests.testNoName
    ]);
  });
};
