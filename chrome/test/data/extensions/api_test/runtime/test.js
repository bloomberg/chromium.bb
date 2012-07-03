// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var fail = chrome.test.fail;
var succeed = chrome.test.succeed;

chrome.test.runTests([

  function testGetURL() {
    if (!chrome.runtime || !chrome.runtime.getURL) {
      fail("chrome.runtime.getURL not defined");
      return;
    }
    var url = chrome.runtime.getURL("_generated_background_page.html");
    assertEq(url, window.location.href);
    succeed();
  },

  function  testGetManifest() {
    if (!chrome.runtime || !chrome.runtime.getManifest) {
      fail("chrome.runtime.getManifest not defined");
      return;
    }
    var manifest = chrome.runtime.getManifest();
    if (!manifest || !manifest.background || !manifest.background.scripts) {
      fail();
      return;
    }
    assertEq(manifest.name, "chrome.runtime API Test");
    assertEq(manifest.version, "1");
    assertEq(manifest.manifest_version, 2);
    assertEq(manifest.background.scripts, ["test.js"]);
    succeed();
  }

]);
