// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function tabsCreateThrowsError() {
    chrome.test.setExceptionHandler(function(_, exception) {
      chrome.test.assertEq('tata', exception.message);
      chrome.test.succeed();
    });
    chrome.tabs.create({}, function() {
      throw new Error('tata');
    });
  },

  function tabsOnCreatedThrowsError() {
    var listener = function() {
      throw new Error('hi');
    };
    chrome.test.setExceptionHandler(function(_, exception) {
      chrome.test.assertEq('hi', exception.message);
      chrome.tabs.onCreated.removeListener(listener);
      chrome.test.succeed();
    });
    chrome.tabs.onCreated.addListener(listener);
    chrome.tabs.create({});
  },

  function permissionsGetAllThrowsError() {
    // permissions.getAll has a custom callback, as do many other methods, but
    // this is easy to call.
    chrome.test.setExceptionHandler(function(_, exception) {
      chrome.test.assertEq('boom', exception.message);
      chrome.test.succeed();
    });
    chrome.permissions.getAll(function() {
      throw new Error('boom');
    });
  }
]);
