// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These are the cookies we expect to see along the way.
var SET_REMOVE_COOKIE = {
  name: 'testSetRemove',
  value: '42',
  domain: 'a.com',
  hostOnly: true,
  path: '/',
  secure: false,
  httpOnly: false,
  session: false,
  expirationDate: 12345678900,
  storeId: "0"
};

var OVERWRITE_COOKIE_PRE = {
  name: 'testOverwrite',
  value: '42',
  domain: 'a.com',
  hostOnly: true,
  path: '/',
  secure: false,
  httpOnly: false,
  session: false,
  expirationDate: 12345678900,
  storeId: "0"
};

var OVERWRITE_COOKIE_POST = {
  name: 'testOverwrite',
  value: '43',
  domain: 'a.com',
  hostOnly: true,
  path: '/',
  secure: false,
  httpOnly: false,
  session: false,
  expirationDate: 12345678900,
  storeId: "0"
};

chrome.test.runTests([
  function testSet() {
    var testCompleted = chrome.test.callbackAdded();
    var listener = function (info) {
      chrome.test.assertFalse(info.removed);
      chrome.test.assertEq('explicit', info.cause);
      chrome.test.assertEq(SET_REMOVE_COOKIE, info.cookie);
      testCompleted();
    };

    chrome.cookies.onChanged.addListener(listener);
    chrome.cookies.set({
      url: 'http://a.com/path',
      name: 'testSetRemove',
      value: '42',
      expirationDate: 12345678900
    }, function () {
      chrome.cookies.onChanged.removeListener(listener);
    });
  },
  function testRemove() {
    var testCompleted = chrome.test.callbackAdded();
    var listener = function (info) {
      chrome.test.assertTrue(info.removed);
      chrome.test.assertEq('explicit', info.cause);
      chrome.test.assertEq(SET_REMOVE_COOKIE, info.cookie);
      testCompleted();
    };

    chrome.cookies.onChanged.addListener(listener);
    chrome.cookies.remove({
      url: 'http://a.com/path',
      name: 'testSetRemove'
    }, function () {
      chrome.cookies.onChanged.removeListener(listener);
    });
  },
  function overwriteFirstSet() {
    var testCompleted = chrome.test.callbackAdded();
    var listener = function (info) {
      chrome.test.assertFalse(info.removed);
      chrome.test.assertEq('explicit', info.cause);
      chrome.test.assertEq(OVERWRITE_COOKIE_PRE, info.cookie);
      testCompleted();
    };

    chrome.cookies.onChanged.addListener(listener);
    chrome.cookies.set({
      url: 'http://a.com/path',
      name: 'testOverwrite',
      value: '42',
      expirationDate: 12345678900
    }, function () {
      chrome.cookies.onChanged.removeListener(listener);
    });
  },
  function overwriteSecondSet() {
    var removeCompleted = chrome.test.callbackAdded();
    var setCompleted = chrome.test.callbackAdded();
    var listenerRemove = function (info) {
      if (info.removed) {
        chrome.test.assertEq('overwrite', info.cause);
        chrome.test.assertEq(OVERWRITE_COOKIE_PRE, info.cookie);
        removeCompleted();
      }
    };
    var listenerSet = function (info) {
      if (!info.removed) {
        chrome.test.assertEq('explicit', info.cause);
        chrome.test.assertEq(OVERWRITE_COOKIE_POST, info.cookie);
        setCompleted();
      }
    };
    chrome.cookies.onChanged.addListener(listenerRemove);
    chrome.cookies.onChanged.addListener(listenerSet);
    chrome.cookies.set({
      url: 'http://a.com/path',
      name: 'testOverwrite',
      value: '43',
      expirationDate: 12345678900
    }, function () {
      chrome.cookies.onChanged.removeListener(listenerRemove);
      chrome.cookies.onChanged.removeListener(listenerSet);
    });
  },
  function overwriteExpired() {
    var setCompleted = chrome.test.callbackAdded();
    var listener = function (info) {
      chrome.test.assertTrue(info.removed);
      chrome.test.assertEq('expired_overwrite', info.cause);
      chrome.test.assertEq(OVERWRITE_COOKIE_POST, info.cookie);
      setCompleted();
    };
    chrome.cookies.onChanged.addListener(listener);
    chrome.cookies.set({
      url: 'http://a.com/path',
      name: 'testOverwrite',
      value: '43',
      expirationDate: 1
    }, function () {
      chrome.cookies.onChanged.removeListener(listener);
    });
  }
]);
