// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.test = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';

// window.* exported functions begin (called by WebViewTest).
window.hasTestPassed = function() {
  return embedder.testStatus == TEST_STATUS.PASSED;
};

window.runGeolocationTest = function(testName) {
  if (!embedder.test.testList[testName]) {
    console.log('Incorrect testName: ' + testName);
    embedder.test.fail();
    return;
  }

  // Run the test.
  embedder.test.testList[testName]();
};
// window.* exported functions end.


/** @private */
embedder.setUp_ = function(config) {
  embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
  embedder.guestURL = embedder.baseGuestURL +
      '/files/extensions/platform_apps/web_view/geolocation' +
      '/geolocation_access_guest.html';
  embedder.iframeURL = embedder.baseGuestURL +
      '/files/extensions/platform_apps/web_view/geolocation' +
      '/geolocation_access_iframe.html';
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
    console.log('No <webview> element created');
    embedder.test.fail();
  }
  return webview;
};

var TEST_STATUS = {
  WAITING: 0,
  FAILED: 1,
  PASSED: 2
};
embedder.testStatus = TEST_STATUS.WAITING;

embedder.test = {};
embedder.test.fail = function() {
  embedder.testStatus = TEST_STATUS.FAILED;
  chrome.test.sendMessage('DoneGeolocationTest');
};
embedder.test.succeed = function() {
  if (embedder.testStatus != TEST_STATUS.FAILED) {
    embedder.testStatus = TEST_STATUS.PASSED;
    chrome.test.sendMessage('DoneGeolocationTest');
  }
};

embedder.test.assertEq = function(a, b) {
  if (a != b) {
    console.log('assertion failed: ' + a + ' != ' + b);
    embedder.test.fail();
  }
};
embedder.test.assertTrue = function(condition) {
  if (!condition) {
    console.log('assertion failed: true != ' + condition);
    embedder.test.fail();
  }
};
embedder.test.assertFalse = function(condition) {
  if (condition) {
    console.log('assertion failed: false != ' + condition);
    embedder.test.fail();
  }
};

/** @private */
embedder.setUpLoadCommit_ = function(webview, testName, opt_iframeURL) {
  var onWebViewLoadCommit = function(e) {
    if (!e.isTopLevel) {
      return;
    }
    // Send post message to <webview> when it's ready to receive them.
    var msgArray = ['check-geolocation-permission', '' + testName];
    if (opt_iframeURL) {
      msgArray.push(opt_iframeURL);
    }
    webview.contentWindow.postMessage(JSON.stringify(msgArray), '*');
  };
  webview.addEventListener('loadcommit', onWebViewLoadCommit);
};


/** @private */
embedder.registerAndWaitForPostMessage_ = function(
    webview, expectedData) {
  var testName = expectedData[0];
  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == '' + testName) {
      embedder.test.assertEq(expectedData.length, data.length);
      for (var i = 0; i < expectedData.length; ++i) {
        embedder.test.assertEq(expectedData[i], data[i]);
      }
      embedder.test.succeed();
    }
  };
  window.addEventListener('message', onPostMessageReceived);
};

/** @private */
embedder.assertCorrectEvent_ = function(e) {
  embedder.test.assertEq('geolocation', e.permission);
  embedder.test.assertTrue(!!e.url);
  embedder.test.assertTrue(e.url.indexOf(embedder.baseGuestURL) == 0);

  // Check that unexpected properties (from other permissionrequest) do not show
  // up in the event object.
  embedder.test.assertFalse('userGesture' in e);
};

// Tests begin.

// Once the guest is allowed or denied geolocation, the guest notifies the
// embedder about the fact via post message.
// The embedder has to initiate a post message so that the guest can get a
// reference to embedder to send the reply back.
//
// We have to run each test (from embedder.test.testList) in a separate platform
// app. Running multiple tests mocking geolocation in a single platform app is
// not reliable due to the nature of ui_test_utils::OverrideGeolocation().
// Therefore we run this tests manually (a.k.a non-RunPlatformAppTest) so we do
// not have to copy the platform app n times for running n JavaScript tests.
// TODO(lazyboy): Look into implementing something similar to
// RunExtensionSubtest() for platform apps.

// Loads a guest which requests geolocation. The embedder (platform app) has
// acccess to geolocation and allows geolocation for the guest.
function testAllow() {
  var webview = embedder.setUpGuest_();

  var onPermissionRequest = function(e) {
    chrome.test.log('Embedder notified on permissionRequest');
    embedder.assertCorrectEvent_(e);
    e.request.allow();
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadCommit_(webview, 'test1');
  // WebViewTest sets (mock) lat, lng to 10, 20.
  embedder.registerAndWaitForPostMessage_(
      webview, ['test1', 'access-granted', 10, 20]);
}

// Loads a guest which requests geolocation. The embedder (platform app) has
// acccess to geolocation, but denies geolocation for the guest.
function testDeny() {
  var webview = embedder.setUpGuest_();

  var onPermissionRequest = function(e) {
    chrome.test.log('Embedder notified on permissionRequest');
    embedder.assertCorrectEvent_(e);
    e.request.deny();
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadCommit_(webview, 'test2');
  embedder.registerAndWaitForPostMessage_(
      webview, ['test2', 'access-denied']);
}

// The guest opens an <iframe>. The <iframe> also requests geolocation.
// This results in geolocation requests go to a BrowserPluginGuest with
// different bridge_id-s.
function testMultipleBridgeIdAllow() {
  var webview = embedder.setUpGuest_();

  var onPermissionRequest = function(e) {
    embedder.assertCorrectEvent_(e);
    e.request.allow();
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadCommit_(webview, 'test3', embedder.iframeURL);
  embedder.registerAndWaitForPostMessage_(
      webview, ['test3', 'access-granted', 10, 20]);
}

embedder.test.testList = {
  'testAllow': testAllow,
  'testDeny': testDeny,
  'testMultipleBridgeIdAllow': testMultipleBridgeIdAllow
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp_(config);
    chrome.test.sendMessage('Launched');
  });
};
