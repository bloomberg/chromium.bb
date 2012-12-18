// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  function simple() {
    chrome.developerPrivate.getItemsInfo(true, // include disabled
                                         true, // include terminated
                                         callback(function(items) {
      chrome.test.assertEq(3, items.length);

      checkItemInList(items, "hosted_app", true, "hosted_app",
          { "app_launch_url": "http://www.google.com/",
            "offline_enabled": true,
            "update_url": "http://example.com/update.xml" });

      checkItemInList(items, "simple_extension", true, "extension",
          { "homepage_url": "http://example.com/",
            "options_url": "chrome-extension://<ID>/pages/options.html"});

      var extension = getItemNamed(items, "packaged_app");
      checkItemInList(items, "packaged_app", true, "packaged_app",
          { "offline_enabled": true});
    }));
  }
];

chrome.test.runTests(tests);
