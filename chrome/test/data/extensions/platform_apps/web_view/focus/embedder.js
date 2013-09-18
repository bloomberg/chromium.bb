// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.tests = {};
embedder.triggerNavUrl =
    'data:text/html,<html><body>trigger navigation<body></html>';

window.runTest = function(testName) {
  if (!embedder.test.testList[testName]) {
    console.log('Incorrect testName: ' + testName);
    embedder.test.fail();
    return;
  }

  // Run the test.
  embedder.test.testList[testName]();
};
// window.* exported functions end.

embedder.test = {};
embedder.test.succeed = function() {
  chrome.test.sendMessage('TEST_PASSED');
};

embedder.test.fail = function() {
  chrome.test.sendMessage('TEST_FAILED');
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
embedder.setUpGuest_ = function() {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"></webview>';
  var webview = document.querySelector('webview');
  if (!webview) {
    embedder.test.fail('No <webview> element created');
  }
  return webview;
};

/** @private */
embedder.waitForResponseFromGuest_ =
    function(webview,
             channelCreationCallback,
             expectedResponse,
             responseCallback) {
  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    var response = data[0];
    if (response == 'connected') {
      channelCreationCallback(webview);
      return;
    }
    if (response != expectedResponse) {
      return;
    }
    responseCallback();
    window.removeEventListener('message', onPostMessageReceived);
  };
  window.addEventListener('message', onPostMessageReceived);

  var onWebViewLoadStop = function(e) {
    console.log('loadstop');
    webview.executeScript(
      {file: 'inject_focus.js'},
      function(results) {
        console.log('Injected script into webview.');
        // Establish a communication channel with the webview1's guest.
        var msg = ['connect'];
        webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      });
    webview.removeEventListener('loadstop', onWebViewLoadStop);
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
  webview.src = embedder.triggerNavUrl;
};

// Tests begin.

// The embedder has to initiate a post message so that the guest can get a
// reference to embedder to send the reply back.

embedder.testFocus_ = function(channelCreationCallback,
                               expectedResponse,
                               responseCallback) {
  var webview = embedder.setUpGuest_();

  embedder.waitForResponseFromGuest_(webview,
                                     channelCreationCallback,
                                     expectedResponse,
                                     responseCallback);
};

function testFocusEvent() {
  var seenResponse = false;
  embedder.testFocus_(function(webview) {
    webview.focus();
  }, 'focused', function() {
    // The focus event fires three times on first focus. We only care about
    // the first focus.
    if (seenResponse) {
      return;
    }
    seenResponse = true;
    embedder.test.succeed();
  });
}

function testBlurEvent() {
  var seenResponse = false;
  embedder.testFocus_(function(webview) {
    webview.focus();
    webview.blur();
  }, 'blurred', function() {
    if (seenResponse) {
      return;
    }
    seenResponse = true;
    embedder.test.succeed();
  });
}

embedder.test.testList = {
  'testFocusEvent': testFocusEvent,
  'testBlurEvent': testBlurEvent
};

onload = function() {
  chrome.test.getConfig(function(config) {
    chrome.test.sendMessage('Launched');
  });
};
