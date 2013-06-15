// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testCallback(callback) {
  var done = chrome.test.callbackAdded();
  return function() {
    try {
      callback.call(null, arguments);
    } finally {
      done();
    }
  };
}

chrome.test.runTests([
  function tabsCreateThrowsError() {
    chrome.tabs.create({}, testCallback(function() {
      throw new Error("tata");
    }));
  },
  function permissionsGetAllThrowsError() {
    // permissions.getAll has a custom callback, as do many other methods, but
    // this is easy to call.
    chrome.permissions.getAll(testCallback(function() {
      throw new Error("boom");
    }));
  }
]);
