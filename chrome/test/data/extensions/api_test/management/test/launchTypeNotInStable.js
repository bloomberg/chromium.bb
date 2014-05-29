// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  function launchTypeNotInStable() {
    assertEq(undefined, chrome.management.setLaunchType);

    chrome.management.getAll(callback(function(items) {
      var app = getItemNamed(items, "enabled_app");
      assertEq(undefined, app.launchType);
      assertEq(undefined, app.availableLaunchTypes);
    }));
  }
];

chrome.test.runTests(tests);
