// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function firstCheck(spatialNavigationEnabled) {
  chrome.test.log('Spatial navigation initially enabled via switch');
  if (spatialNavigationEnabled) {
    chrome.test.sendMessage('TEST_STEP_PASSED');
  } else {
    chrome.test.sendMessage('TEST_STEP_FAILED');
  }
}

function secondCheck(e) {
  chrome.test.log('After RIGHT key is pressed once');
  if (e.message == 'focused:1') {
    // setup next tests
    var webview = document.querySelector('webview');
    webview.removeEventListener('consolemessage', secondCheck);
    webview.addEventListener('consolemessage', thirdCheck);

    chrome.test.sendMessage('TEST_STEP_PASSED');
  } else {
    chrome.test.sendMessage('TEST_STEP_FAILED');
  }
}

function thirdCheck(e) {
  chrome.test.log('After RIGHT key is pressed once more');
  if (e.message == 'focused:2') {
    // setup next tests
    var webview = document.querySelector('webview');
    webview.removeEventListener('consolemessage', thirdCheck);
    webview.setSpatialNavigationEnabled(false);
    webview.isSpatialNavigationEnabled(fourthCheck);
    webview.addEventListener('consolemessage', fifthCheck);

    chrome.test.sendMessage('TEST_STEP_PASSED');
  } else {
    chrome.test.sendMessage('TEST_STEP_FAILED');
  }
}

function fourthCheck(spatialNavigationEnabled) {
  chrome.test.log('Spatial navigation disabled');
  if (spatialNavigationEnabled) {
    chrome.test.sendMessage('TEST_STEP_FAILED');
  } else {
    chrome.test.sendMessage('TEST_STEP_PASSED');
  }
}

function fifthCheck(e) {
  chrome.test.log('After RIGHT key is pressed once, then SHIFT+TAB');
  if (e.message == 'focused:1') {
    chrome.test.sendMessage('TEST_STEP_PASSED');
  } else {
    chrome.test.sendMessage('TEST_STEP_FAILED');
  }
}

function startTest() {
  chrome.test.log('Webview initializing');
  var webview = document.querySelector('webview');
  var onLoadStop = function(e) {
    chrome.test.log('Webview initialized');
    webview.removeEventListener('loadstop', onLoadStop);
    chrome.test.sendMessage('WebViewTest.LAUNCHED');

    webview.isSpatialNavigationEnabled(firstCheck);
    webview.focus();
  };

  webview.addEventListener('loadstop', onLoadStop);
  webview.addEventListener('consolemessage', secondCheck);

  webview.src = `data:text/html,
    <body>
      <a id='1' href='%23'>b</a>
      <a id='2' href='%23'>c</a>
      <a id='3' href='%23'>d</a>
      <a id='4' href='%23'>e</a>
      <a id='5' href='%23'>f</a>
      <a id='6' href='%23'>g</a>
      <a id='7' href='%23'>h</a>
      <a id='8' href='%23'>i</a>
      <script>
        document.querySelectorAll('a').forEach(function(each) {
          each.onfocus = function(ev) {
            console.log('focused:'+ev.target.id);
          };
        });
        </script>
    </body>
  `;
};

window.onload = function() {
  startTest();
};
