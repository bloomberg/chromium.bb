// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function requestUrl(url, callback) {
  var req = new XMLHttpRequest();
  req.open('GET', url, true);
  req.onload = function() {
    if (req.status == 200)
      callback(req.responseText);
    else
      req.onerror();
  };
  req.onerror = function() {
    chrome.test.fail('XHR failed: ' + req.status);
  };
  req.send(null);
}

var REMOTE_DEBUGGER_HOST = 'localhost:9222';

function checkTarget(targets, url, type, opt_title, opt_faviconUrl) {
  var target =
      targets.filter(function(t) { return t.url == url }) [0];
  if (!target)
    chrome.test.fail('Cannot find a target with url ' + url);

  var wsAddress = REMOTE_DEBUGGER_HOST + '/devtools/page/' + target.id;

  chrome.test.assertEq(
      '/devtools/devtools.html?ws=' + wsAddress,
      target.devtoolsFrontendUrl);
  // On some platforms (e.g. Chrome OS) target.faviconUrl might be empty for
  // a freshly created tab. Ignore the check then.
  if (target.faviconUrl)
    chrome.test.assertEq(opt_faviconUrl, target.faviconUrl);
  chrome.test.assertEq('/thumb/' + target.id, target.thumbnailUrl);
  chrome.test.assertEq(opt_title || target.url, target.title);
  chrome.test.assertEq(type, target.type);
  chrome.test.assertEq('ws://' + wsAddress, target.webSocketDebuggerUrl);
}

chrome.test.runTests([
  function discoverTargets() {
    var testPageUrl = chrome.extension.getURL('test_page.html');

    function onUpdated() {
      chrome.tabs.onUpdated.removeListener(onUpdated);
      requestUrl('http://' + REMOTE_DEBUGGER_HOST + '/json', function(text) {
        var targets = JSON.parse(text);

        checkTarget(targets, 'about:blank', 'page');
        checkTarget(targets, testPageUrl, 'page', 'Test page',
            chrome.extension.getURL('favicon.png'));
        checkTarget(targets,
            chrome.extension.getURL('_generated_background_page.html'),
            'other');

        chrome.test.succeed();
      });
    }
    chrome.tabs.onUpdated.addListener(onUpdated);
    chrome.tabs.create({url: testPageUrl});
  }
]);
