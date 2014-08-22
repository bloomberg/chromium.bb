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
  },
  function copyInIframe() {
    var ifr = document.createElement('iframe');
    document.body.appendChild(ifr);
    window.command = 'copy';
    ifr.contentDocument.write('<script src="iframe.js"></script>');
  },
  function pasteInIframe() {
    var ifr = document.createElement('iframe');
    document.body.appendChild(ifr);
    window.command = 'paste';
    ifr.contentDocument.write('<script src="iframe.js"></script>');
  }
]);

function testDone(result) {
  // 'copy' should always succeed regardless of the clipboardWrite permission,
  // for backwards compatibility. 'paste' should always fail because the
  // extension doesn't have clipboardRead.
  var expected = window.command === 'copy';
  if (result === expected)
    chrome.test.succeed();
  else
    chrome.test.fail();
}
