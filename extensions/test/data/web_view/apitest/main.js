// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};

// TODO(lfg) Move these functions to a common js.
window.runTest = function(testName) {
  if (!embedder.test.testList[testName]) {
    window.console.warn('Incorrect testName: ' + testName);
    embedder.test.fail();
    return;
  }

  // Run the test.
  embedder.test.testList[testName]();
};

embedder.test = {};

embedder.test.assertEq = function(a, b) {
  if (a != b) {
    window.console.warn('assertion failed: ' + a + ' != ' + b);
    embedder.test.fail();
  }
};

embedder.test.assertFalse = function(condition) {
  if (condition) {
    window.console.warn('assertion failed: false != ' + condition);
    embedder.test.fail();
  }
};

embedder.test.assertTrue = function(condition) {
  if (!condition) {
    window.console.warn('assertion failed: true != ' + condition);
    embedder.test.fail();
  }
};

embedder.test.fail = function() {
  chrome.test.sendMessage('TEST_FAILED');
};

embedder.test.succeed = function() {
  chrome.test.sendMessage('TEST_PASSED');
};


// Tests begin.

function testAPIMethodExistence() {
  var apiMethodsToCheck = [
    'back',
    'find',
    'forward',
    'canGoBack',
    'canGoForward',
    'clearData',
    'getProcessId',
    'getZoom',
    'go',
    'print',
    'reload',
    'setZoom',
    'stop',
    'stopFinding',
    'terminate',
    'executeScript',
    'insertCSS',
    'getUserAgent',
    'isUserAgentOverridden',
    'setUserAgentOverride'
  ];
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  webview.addEventListener('loadstop', function(e) {
    for (var i = 0; i < apiMethodsToCheck.length; ++i) {
      embedder.test.assertEq('function',
                             typeof webview[apiMethodsToCheck[i]]);
    }

    // Check contentWindow.
    embedder.test.assertEq('object', typeof webview.contentWindow);
    embedder.test.assertEq('function',
                           typeof webview.contentWindow.postMessage);
    embedder.test.succeed();
  });
  webview.setAttribute('src', 'data:text/html,webview check api');
  document.body.appendChild(webview);
}

embedder.test.testList = {
  'testAPIMethodExistence': testAPIMethodExistence
};

onload = function() {
  chrome.test.sendMessage('LAUNCHED');
};
