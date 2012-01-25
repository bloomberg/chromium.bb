// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var kPermissionError = "Extension does not have the permission to use this API";

chrome.test.runTests([
  function tryOpenExtension() {
    chrome.terminalPrivate.openTerminalProcess("crosh",
        chrome.test.callbackFail(kPermissionError));
  },
  function trySendInput() {
    chrome.terminalPrivate.sendInput(1222, "some input",
        chrome.test.callbackFail(kPermissionError));
  },
  function tryClose() {
    chrome.terminalPrivate.closeTerminalProcess(1222,
        chrome.test.callbackFail(kPermissionError));
  }
]);
