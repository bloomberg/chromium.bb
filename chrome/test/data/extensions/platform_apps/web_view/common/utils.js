// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var $ = function(id) { return document.getElementById(id); };
var LOG = function(msg) { window.console.log(msg); };

var embedder = {};
embedder.guestURL = '';
embedder.webview = null;

// This is filled in by the embedder bootstrap.js.
var config = {};

var utils = {};
utils.test = {};

// Note that chrome.test.succeed() and chrome.test.fail() is wrapped below
// so that we can run manual tests (i.e. sending custom
// chrome.test.sendMessage-s to pass/fail test) once we start running them
// through this common code.
utils.test.succeed = function() {
  LOG('utils.test.succeed');
  chrome.test.succeed();
};

utils.test.fail = function(opt_msg) {
  LOG('utils.test.fail, test failure: ' + opt_msg || '');
  chrome.test.fail(opt_msg || '');
};

utils.test.assertEq = function(expected, actual) {
  chrome.test.assertEq.apply(chrome.test.assertEq, arguments);
};

utils.setUp = function(chromeConfig, config) {
  if (config.IS_JS_ONLY_GUEST) {
    embedder.guestURL = 'about:blank';
  } else {
    var baseGuestURL = 'http://localhost:' + chromeConfig.testServer.port;
    embedder.guestURL = baseGuestURL +
        '/extensions/platform_apps/web_view/common/' +
        config.TEST_DIR + '/guest.html';
  }
  chrome.test.log('Guest url is: ' + embedder.guestURL);
};

utils.injectCss = function(cssScriptPath) {
  LOG('BEG utils.injectCss: ' + cssScriptPath);
  var style = document.createElement('style');
  style.type = 'text/css';
  style.src = cssScriptPath;
  document.getElementsByTagName('head')[0].appendChild(style);
  LOG('END utils.injectCss');
};

embedder.loadGuest = function(
    connectedCallback, postMessageCallback, opt_partitionName,
    opt_preLoadHooks) {
  LOG('embedder.loadGuest begin');
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"></webview>';
  embedder.webview = document.querySelector('webview');
  if (opt_partitionName) {
    embedder.webview.partition = opt_partitionName;
  }
  if (!embedder.webview) {
    utils.test.fail('No <webview> element created');
    return;
  }

  embedder.webview.addEventListener('consolemessage', function(e) {
    LOG('FROM GUEST: ' + e.message);
  });

  // Step 1. loadstop.
  embedder.webview.addEventListener('loadstop', function(e) {
    LOG('embedder.webview.loadstop');

    LOG('IS_JS_ONLY_GUEST: ' + config.IS_JS_ONLY_GUEST);
    if (config.IS_JS_ONLY_GUEST) {
      // We do not have a TestServer, we load a guest pointing to
      // about:blank and inject script to it.
      LOG('embedder.webview.inject');
      embedder.webview.executeScript(
          {file: config.TEST_DIR + '/guest.js'},
          function(results) {
            if (!results || !results.length) {
              LOG('Error injecting JavaScript to guest');
              utils.test.fail();
              return;
            }
            embedder.webview.contentWindow.postMessage(
                JSON.stringify(['create-channel']), '*');
          });
    } else {
      embedder.webview.contentWindow.postMessage(
          JSON.stringify(['create-channel']), '*');
    }
  });

  // Step 2. Receive postMessage.
  var onPostMessageReceived = function(e) {
    LOG('embedder.webview.onPostMessageReceived');
    var data = JSON.parse(e.data);
    var response = data[0];
    if (response == 'channel-created') {
      connectedCallback(embedder.webview);
    } else {
      if (!postMessageCallback(data)) {
        chrome.test.log('Unexpected response from guest');
        utils.test.fail();
      }
    }
  };

  if (opt_preLoadHooks) {
    opt_preLoadHooks(embedder.webview);
  }

  window.addEventListener('message', onPostMessageReceived);
  embedder.webview.setAttribute('src', embedder.guestURL);
};

