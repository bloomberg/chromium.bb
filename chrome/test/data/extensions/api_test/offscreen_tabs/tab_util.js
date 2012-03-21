// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var assertFalse = chrome.test.assertFalse;

var Q_KEY = 113;

function assertEqTabs(tab1, tab2) {
  assertEq(tab1.id, tab2.id);
  assertSimilarTabs(tab1, tab2);
}

function maybeExpandURL(url) {
  if (url.match('chrome-extension'))
    return url;
  return chrome.extension.getURL(url);
}

function assertSimilarTabs(tab1, tab2) {
  assertEq(maybeExpandURL(tab1.url), maybeExpandURL(tab2.url));
  assertEq(tab1.width, tab2.width);
  assertEq(tab1.height, tab2.height);
}

function assertTabDoesNotExist(tabId) {
  var expectedError = 'No offscreen tab with id: ' + tabId + '.';
  chrome.experimental.offscreenTabs.get(tabId, fail(expectedError));
}

function getBaseMouseEvent() {
  return {
    button: 0,
    altKey: false,
    ctrlKey: false,
    shiftKey: false
  };
};

function getKeyboardEvent(keyCode) {
  return {
    type: 'keypress',
    charCode: keyCode,
    keyCode: keyCode,
    altKey: false,
    ctrlKey: false,
    shiftKey: false
  };
}
