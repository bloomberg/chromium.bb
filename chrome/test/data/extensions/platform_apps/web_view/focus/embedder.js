// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var g_webview = null;
var embedder = {};
var seenFocusCount = 0;
embedder.tests = {};
embedder.guestURL =
    'data:text/html,<html><body>Guest<body></html>';

window.runTest = function(testName) {
  if (!embedder.test.testList[testName]) {
    console.log('Incorrect testName: ' + testName);
    embedder.test.fail();
    return;
  }

  // Run the test.
  embedder.test.testList[testName]();
};

window.runCommand = function(command) {
  window.console.log('window.runCommand: ' + command);
  switch (command) {
    case 'POST_testFocusTracksEmbedder':
      POST_testFocusTracksEmbedder();
      break;
    default:
      embedder.test.fail();
  }
};
// window.* exported functions end.

var LOG = function(msg) {
  window.console.log(msg);
};

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
    responseCallback(data);
    window.removeEventListener('message', onPostMessageReceived);
  };
  window.addEventListener('message', onPostMessageReceived);

  webview.addEventListener('consolemessage', function(e) {
    LOG('g: ' + e.message);
  });

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
  webview.src = embedder.guestURL;
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

// Verifies that if a <webview> is focused before navigation then the guest
// starts off focused.
//
// We create a <webview> element and make it focused before navigating it.
// Then we load a URL in it and make sure document.hasFocus() returns true
// for the <webview>.
function testFocusBeforeNavigation() {
  var webview = document.createElement('webview');
  document.body.appendChild(webview);

  var onChannelEstablished = function(webview) {
    // Query the guest if it has focus.
    var msg = ['request-hasFocus'];
    webview.contentWindow.postMessage(JSON.stringify(msg), '*');
  };

  // Focus the <webview> before navigating it.
  webview.focus();

  embedder.waitForResponseFromGuest_(
    webview,
    onChannelEstablished,
    'response-hasFocus',
    function(data) {
      LOG('data, hasFocus: ' + data[1]);
      embedder.test.assertEq(true, data[1]);
      embedder.test.succeed();
    });
}

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

// Tests that if we focus/blur the embedder, it also gets reflected in the
// guest.
//
// This test has two steps:
// 1) testFocusTracksEmbedder(), in this step we create a <webview> and
// focus it before navigating. After navigating it to a URL, we focus an input
// element inside the <webview>, and wait for its 'focus' event to fire.
// 2) POST_testFocusTracksEmbedder(), in this step, we have already called
// Blur() on the embedder's RVH (see WebViewTest.Focus_FocusTracksEmbedder),
// we make sure we see a 'blur' event on the <webview>'s input element.
function testFocusTracksEmbedder() {
  var webview = document.createElement('webview');
  g_webview = webview;
  document.body.appendChild(webview);

  var onChannelEstablished = function(webview) {
    var msg = ['request-waitForFocus'];
    webview.contentWindow.postMessage(JSON.stringify(msg), '*');
  };

  // Focus the <webview> before navigating it.
  // This is necessary so that 'blur' event on guest's <input> element fires.
  webview.focus();

  embedder.waitForResponseFromGuest_(
      webview,
      onChannelEstablished,
      'response-seenFocus',
      function(data) { embedder.test.succeed(); });
}

// Runs the second step for testFocusTracksEmbedder().
// See WebViewTest.Focus_FocusTracksEmbedder() to see how this is invoked.
function POST_testFocusTracksEmbedder() {
  g_webview.contentWindow.postMessage(
      JSON.stringify(['request-waitForBlurAfterFocus']), '*');

  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    LOG('send window.message, data: ' + data);
    if (data[0] == 'response-seenBlurAfterFocus') {
      chrome.test.sendMessage('POST_TEST_PASSED');
    } else {
      chrome.test.sendMessage('POST_TEST_FAILED');
    }
  });
}

// Tests that <webview> sees advanceFocus() call when we cycle through the
// elements inside it using tab key.
//
// This test has two steps:
// 1) testAdvanceFocus(), in this step, we focus the embedder and press a
// tab key, we expect the input element inside the <webview> to be focused.
// 2) POST_testAdvanceFocus(), in this step we send additional tab keypress
// to the embedder/app (from WebViewInteractiveTest.Focus_AdvanceFocus), this
// would cycle the focus within the elements and will bring focus back to
// the input element present in the <webview> mentioned in step 1.
function testAdvanceFocus() {
  var webview = document.createElement('webview');
  g_webview = webview;
  document.body.appendChild(webview);

  webview.addEventListener('consolemessage', function(e) {
    LOG('g: ' + e.message);
  });
  webview.addEventListener('loadstop', function(e) {
    LOG('loadstop');

    window.addEventListener('message', function(e) {
      var data = JSON.parse(e.data);
      LOG('message, data: ' + data);

      if (data[0] == 'connected') {
        embedder.test.succeed();
      } else if (data[0] == 'button1-focused') {
        var focusCount = data[1];
        LOG('focusCount: ' + focusCount);
        seenFocusCount++;
        if (focusCount == 1) {
          chrome.test.sendMessage('button1-focused');
        } else {
          chrome.test.sendMessage('button1-advance-focus');
        }
      }
    });

    webview.executeScript(
      {file: 'inject_advance_focus_test.js'},
      function(results) {
        window.console.log('webview.executeScript response');
        if (!results || !results.length) {
          LOG('Inject script failure.');
          embedder.test.fail();
          return;
        }
        webview.contentWindow.postMessage(JSON.stringify(['connect']), '*');
      });
  });

  webview.src = embedder.guestURL;
}

embedder.test.testList = {
  'testAdvanceFocus': testAdvanceFocus,
  'testFocusBeforeNavigation': testFocusBeforeNavigation,
  'testFocusEvent': testFocusEvent,
  'testFocusTracksEmbedder': testFocusTracksEmbedder,
  'testBlurEvent': testBlurEvent
};

onload = function() {
  chrome.test.getConfig(function(config) {
    chrome.test.sendMessage('Launched');
  });
};
