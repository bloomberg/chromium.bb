// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// systeminfo.memory api test
// browser_tests --gtest_filter=SystemInfoMemoryApiTest.*

chrome.systemInfo = chrome.experimental.systemInfo;

chrome.test.runTests([
  function testGet() {
    for(var i = 0; i < 10; ++i) {
      chrome.systemInfo.display.get(chrome.test.callbackPass(function(result) {
        chrome.test.assertEq(2, result.length);
        for (var i = 0; i < result.length; i++) {
          var info = result[i];
          chrome.test.assertEq('DISPLAY', info.id);
          chrome.test.assertEq(i, info.index);
          chrome.test.assertEq(i == 0 ? true : false, info.isPrimary);
          chrome.test.assertEq(0, info.availTop);
          chrome.test.assertEq(0, info.availLeft);
          chrome.test.assertEq(960, info.availWidth);
          chrome.test.assertEq(720, info.availHeight);
          chrome.test.assertEq(32, info.colorDepth);
          chrome.test.assertEq(32, info.pixelDepth);
          chrome.test.assertEq(1280, info.height);
          chrome.test.assertEq(720, info.width);
          chrome.test.assertEq(0, info.absoluteTopOffset);
          chrome.test.assertEq(1280, info.absoluteLeftOffset);
          chrome.test.assertEq(96, info.dpiX);
          chrome.test.assertEq(96, info.dpiY);
        }
      }));
    }
  }
]);

