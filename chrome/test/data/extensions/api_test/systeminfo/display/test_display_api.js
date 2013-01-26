// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// systeminfo.memory api test
// browser_tests --gtest_filter=SystemInfoMemoryApiTest.*

chrome.test.runTests([
  function testGet() {
    for(var i = 0; i < 10; ++i) {
      chrome.systemInfo.display.getDisplayInfo(
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(2, result.length);
          for (var i = 0; i < result.length; i++) {
            var info = result[i];
            chrome.test.assertEq('DISPLAY', info.id);
            chrome.test.assertEq('DISPLAY NAME', info.name);
            chrome.test.assertEq(i == 0 ? true : false, info.isPrimary);
            chrome.test.assertEq(i == 0 ? true : false, info.isInternal);
            chrome.test.assertEq(true, info.isEnabled);
            chrome.test.assertEq(96.0, info.dpiX);
            chrome.test.assertEq(96.0, info.dpiY);
            chrome.test.assertEq(0, info.bounds.left);
            chrome.test.assertEq(0, info.bounds.top);
            chrome.test.assertEq(1280, info.bounds.width);
            chrome.test.assertEq(720, info.bounds.height);
            chrome.test.assertEq(0, info.workArea.left);
            chrome.test.assertEq(0, info.workArea.top);
            chrome.test.assertEq(960, info.workArea.width);
            chrome.test.assertEq(720, info.workArea.height);
        }
      }));
    }
  }
]);

