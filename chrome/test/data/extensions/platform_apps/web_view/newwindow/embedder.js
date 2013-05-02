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
      '/guest_opener.html';
  chrome.test.log('Guest url is: ' + embedder.guestURL);
};

/** @private */
embedder.setUpGuest_ = function(partitionName) {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"></webview>';
  var webview = document.querySelector('webview');
  if (partitionName) {
    webview.partition = partitionName;
  }
  if (!webview) {
    chrome.test.fail('No <webview> element created');
  }
  return webview;
};

/** @private */
embedder.setUpNewWindowRequest_ = function(webview, url, frameName, testName) {
  var onWebViewLoadStop = function(e) {
    // Send post message to <webview> when it's ready to receive them.
    var redirect = testName.indexOf("_blank") != -1;
    webview.contentWindow.postMessage(
        JSON.stringify(
            ['open-window', '' + url, '' + frameName, redirect]), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
  webview.setAttribute('src', embedder.guestURL);
};

/** @private */
embedder.requestFrameName_ =
    function(webview, openerWebview, testName, expectedFrameName) {
  var onWebViewLoadStop = function(e) {
    // Send post message to <webview> when it's ready to receive them.
    // Note that loadstop will get called twice if the test is opening
    // a new window via a redirect: one for navigating to about:blank
    // and another for navigating to the final destination.
    // about:blank will not respond to the postMessage so it's OK
    // to send it again on the second loadstop event.
    webview.contentWindow.postMessage(
        JSON.stringify(['get-frame-name', testName]), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == 'get-frame-name') {
      var name = data[1];
      if (testName != name)
        return;
      var frameName = data[2];
      chrome.test.assertEq(expectedFrameName, frameName);
      chrome.test.assertEq(expectedFrameName, webview.name);
      chrome.test.assertEq(openerWebview.partition, webview.partition);
      window.removeEventListener('message', onPostMessageReceived);
      chrome.test.succeed();
    }
  };
  window.addEventListener('message', onPostMessageReceived);
};

/** @private */
embedder.requestClose_ = function(webview, testName) {
  var onWebViewLoadStop = function(e) {
    webview.contentWindow.postMessage(
        JSON.stringify(['close', testName]), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
  var onWebViewClose = function(e) {
    webview.removeEventListener('close', onWebViewClose);
    chrome.test.succeed();
  };
  webview.addEventListener('close', onWebViewClose);
};

/** @private */
embedder.assertCorrectEvent_ = function(e) {
  chrome.test.assertEq('newwindow', e.type);
  chrome.test.assertTrue(!!e.targetUrl);
};

// Tests begin.

var testNewWindowName = function(testName,
                                 webViewName,
                                 guestName,
                                 partitionName,
                                 expectedFrameName) {
  var webview = embedder.setUpGuest_(partitionName);

  var onNewWindow = function(e) {
    chrome.test.log('Embedder notified on newwindow');
    embedder.assertCorrectEvent_(e);

    var newwebview = document.createElement('webview');
    newwebview.setAttribute('name', webViewName);
    document.querySelector('#webview-tag-container').appendChild(newwebview);
    embedder.requestFrameName_(
        newwebview, webview, testName, expectedFrameName);
    try {
      e.window.attach(newwebview);
    } catch (e) {
      chrome.test.fail();
    }
  };
  webview.addEventListener('newwindow', onNewWindow);

  // Load a new window with the given name.
  embedder.setUpNewWindowRequest_(webview, 'guest.html', guestName, testName);
};

// Loads a guest which requests a new window.
embedder.tests.testNewWindowNameTakesPrecedence =
    function testNewWindowNameTakesPrecedence() {
  var webViewName = 'foo';
  var guestName = 'bar';
  var partitionName = 'foobar';
  var expectedName = guestName;
  testNewWindowName('testNewWindowNameTakesPrecedence',
                    webViewName, guestName, partitionName, expectedName);
};

embedder.tests.testWebViewNameTakesPrecedence =
    function testWebViewNameTakesPrecedence() {
  var webViewName = 'foo';
  var guestName = '';
  var partitionName = 'persist:foobar';
  var expectedName = webViewName;
  testNewWindowName('testWebViewNameTakesPrecedence',
                    webViewName, guestName, partitionName, expectedName);
};

embedder.tests.testNoName = function testNoName() {
  var webViewName = '';
  var guestName = '';
  var partitionName = '';
  var expectedName = '';
  testNewWindowName('testNoName',
                    webViewName, guestName, partitionName, expectedName);
};

embedder.tests.testNewWindowRedirect =
    function testNewWindowRedirect() {
  var webViewName = 'foo';
  var guestName = '';
  var partitionName = 'persist:foobar';
  var expectedName = webViewName;
  testNewWindowName('testNewWindowRedirect_blank',
                    webViewName, guestName, partitionName, expectedName);
};

embedder.tests.testNewWindowClose = function testNewWindowClose() {
  var testName = 'testNewWindowClose';
  var webview = embedder.setUpGuest_('foobar');

  var onNewWindow = function(e) {
    chrome.test.log('Embedder notified on newwindow');
    embedder.assertCorrectEvent_(e);

    var newwebview = document.createElement('webview');
    document.querySelector('#webview-tag-container').appendChild(newwebview);
    embedder.requestClose_(newwebview, testName);
    try {
      e.window.attach(newwebview);
    } catch (e) {
      chrome.test.fail();
    }
  };
  webview.addEventListener('newwindow', onNewWindow);

  // Load a new window with the given name.
  embedder.setUpNewWindowRequest_(webview, 'guest.html', '', testName);
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp(config);
    chrome.test.runTests([
      embedder.tests.testNewWindowNameTakesPrecedence,
      embedder.tests.testWebViewNameTakesPrecedence,
      embedder.tests.testNoName,
      embedder.tests.testNewWindowRedirect,
      embedder.tests.testNewWindowClose
    ]);
  });
};
