// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.tests = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';

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
        JSON.stringify(['check-media-permission', '' + testName]), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
};

/** @private */
embedder.registerAndWaitForPostMessage_ = function(webview, testName) {
  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    if (data[1] == '' + testName) {
      chrome.test.assertEq('access-denied', data[0]);
      chrome.test.succeed();
    }
  };
  window.addEventListener('message', onPostMessageReceived);
};

/** @private */
embedder.assertCorrectMediaEvent_ = function(e) {
  chrome.test.assertEq('media', e.permission);
  chrome.test.assertTrue(!!e.url);
  chrome.test.assertTrue(e.url.indexOf(embedder.baseGuestURL) == 0);
};

// Each test loads a guest which requests media access.
// All tests listed in this file expect the guest's request to be denied.
//
// Once the guest is denied media access, the guest notifies the embedder about
// the fact via post message.
// The embedder has to initiate a post message so that the guest can get a
// reference to embedder to send the reply back.

embedder.tests.testDeny = function testDeny() {
  var webview = embedder.setUpGuest_();

  var onPermissionRequest = function(e) {
    chrome.test.log('Embedder notified on permissionRequest');
    chrome.test.assertEq('media', e.permission);
    chrome.test.assertTrue(!!e.url);
    chrome.test.assertTrue(e.url.indexOf(embedder.baseGuestURL) == 0);

    e.request.deny();
    // Calling allow/deny after this should raise exception.
    ['allow', 'deny'].forEach(function(funcName) {
      var exceptionCaught = false;
      try {
        e.request[funcName]();
      } catch (exception) {
        exceptionCaught = true;
      }
      if (!exceptionCaught) {
        chrome.test.fail('Expected exception on multiple e.allow()');
      }
    });
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadStop_(webview, 'test1');
  embedder.registerAndWaitForPostMessage_(webview, 'test1');
};

embedder.tests.testDenyThenAllowThrows = function testDenyThenAllowThrows() {
  var webview = embedder.setUpGuest_();

  var exceptionCount = 0;
  var callCount = 0;
  var denyCalled = false;
  var callDenyAndCheck = function(e) {
    try {
      if (!denyCalled) {
        e.request.deny();
        denyCalled = true;
      } else {
        e.request.allow();
      }
      ++callCount;
    } catch (ex) {
      ++exceptionCount;
    }
    if (callCount == 1 && exceptionCount == 1) {
      chrome.test.succeed();
    }
  };

  var onPermissionRequest1 = function(e) {
    embedder.assertCorrectMediaEvent_(e);
    callDenyAndCheck(e);
  };
  var onPermissionRequest2 = function(e) {
    embedder.assertCorrectMediaEvent_(e);
    callDenyAndCheck(e);
  };
  webview.addEventListener('permissionrequest', onPermissionRequest1);
  webview.addEventListener('permissionrequest', onPermissionRequest2);

  embedder.setUpLoadStop_(webview, 'test2');
  embedder.registerAndWaitForPostMessage_(webview, 'test2');
};

embedder.tests.testDenyWithPreventDefault =
    function testDenyWithPreventDefault() {
  var webview = embedder.setUpGuest_();

  var onPermissionRequest = function(e) {
    embedder.assertCorrectMediaEvent_(e);
    e.preventDefault();
    // Deny asynchronously.
    window.setTimeout(function() { e.request.deny(); });
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadStop_(webview, 'test3');
  embedder.registerAndWaitForPostMessage_(webview, 'test3');
};

embedder.tests.testNoListenersImplyDeny = function testNoListenersImplyDeny() {
  var webview = embedder.setUpGuest_();
  embedder.setUpLoadStop_(webview, 'test4');
  embedder.registerAndWaitForPostMessage_(webview, 'test4');
};

embedder.tests.testNoPreventDefaultImpliesDeny =
    function testNoPreventDefaultImpliesDeny() {
  var webview = embedder.setUpGuest_();

  var onPermissionRequest = function(e) {
    embedder.assertCorrectMediaEvent_(e);
    window.setTimeout(function() {
      // Allowing asynchronously. Since we didn't call preventDefault(), the
      // request will be denied before we get here.
      try {
        e.request.allow();
      } catch (exception) {
        // Ignore.
      }
    });
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  embedder.setUpLoadStop_(webview, 'test5');
  embedder.registerAndWaitForPostMessage_(webview, 'test5');
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
    embedder.guestURL = embedder.baseGuestURL +
        '/files/extensions/platform_apps/web_view/media_access' +
        '/media_access_guest.html';
    chrome.test.log('Guest url is: ' + embedder.guestURL);

    chrome.test.runTests([
      embedder.tests.testDeny,
      embedder.tests.testDenyThenAllowThrows,
      embedder.tests.testDenyWithPreventDefault,
      embedder.tests.testNoListenersImplyDeny,
      embedder.tests.testNoPreventDefaultImpliesDeny
    ]);
  });
};
