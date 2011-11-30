// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Clipboard permission test for Chrome.
// browser_tests.exe --gtest_filter=ClipboardApiTest.ExtensionNoPermission

chrome.test.runTests([
  function domCopy() {
    if (document.execCommand('copy'))
      chrome.test.succeed();
    else
      chrome.test.fail('execCommand("copy") failed');
  },
  function domPaste() {
    if (!document.execCommand('paste'))
      chrome.test.succeed();
    else
      chrome.test.fail('execCommand("paste") succeeded');
  }
]);
