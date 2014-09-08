// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// All these tests are run in Stable channel.

var error = "The visibleOnAllWorkspaces option requires dev channel or newer.";

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([

    // Check CreateWindowOptions.visibleOnAllWorkspaces().
    function testCreateOption() {
      chrome.app.window.create(
          'index.html', {
            visibleOnAllWorkspaces: true,
          }, chrome.test.callbackFail(error));
    },

    // Check chrome.app.window.canSetVisibleOnAllWorkspaces().
    function testCanSetVisibleOnAllWorkspaces() {
      chrome.test.assertTrue(
          chrome.app.window.canSetVisibleOnAllWorkspaces === undefined);
      chrome.test.callbackPass(function () {})();
    },

  ]);
});
