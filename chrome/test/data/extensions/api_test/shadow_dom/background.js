// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function currentWindow() {
    chrome.test.assertTrue('WebKitShadowRoot' in window);
    chrome.test.succeed();
  },

  function newWindow() {
    var w = window.open('empty.html');
    chrome.test.assertTrue('WebKitShadowRoot' in w);
    chrome.test.succeed();
  }
]);
