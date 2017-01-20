// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;
var callbackFail = chrome.test.callbackFail;
var callbackPass = chrome.test.callbackPass;

var RoleType = chrome.automation.RoleType;

function canXhr(url) {
  assertFalse(url == null);
  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, false);
  var success = true;
  try {
    xhr.send();
  } catch(e) {
    assertEq('NetworkError', e.name);
    success = false;
  }
  return success;
}

var cachedUrl = null;
var iframeDone = null;

chrome.runtime.onMessage.addListener(function(request, sender, sendResponse) {
  if (request.message == 'xhr') {
    sendResponse({url: cachedUrl});
  } else {
    assertTrue(request.success);
    iframeDone();
  }
});

var iframeUrl = chrome.extension.getURL('iframe.html');
var injectIframe =
    'var iframe = document.createElement("iframe");\n' +
    'iframe.src = "' + iframeUrl + '";\n' +
    'document.body.appendChild(iframe);\n';

chrome.browserAction.onClicked.addListener(function(tab) {
  iframeDone = chrome.test.callbackAdded();
  cachedUrl = tab.url;
  chrome.tabs.executeScript({ code: injectIframe }, callbackPass());
  assertTrue(canXhr(tab.url));

  chrome.automation.getTree(callbackPass(function(rootNode) {
    assertFalse(rootNode == undefined);
    assertEq(RoleType.ROOT_WEB_AREA, rootNode.role);
  }));
});

chrome.webNavigation.onCompleted.addListener(function(details) {
  chrome.tabs.executeScript({ code: 'true' }, callbackFail(
         'Cannot access contents of the page. ' +
         'Extension manifest must request permission to access the ' +
         'respective host.'));

  chrome.automation.getTree(callbackFail(
      'Cannot request automation tree on url "' + details.url +
        '". Extension manifest must request permission to access this host.'));

  assertFalse(canXhr(details.url));
});
