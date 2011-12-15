// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTypes = {
  cache: true,
  cookies: true,
  downloads: true,
  formData: true,
  history: true,
  passwords: true
};
chrome.test.runTests([
  function testClearEverything() {
    chrome.experimental.clear.browsingData("everything", allTypes,
        chrome.test.callbackPass());
  },
  function testClearCache() {
    chrome.experimental.clear.cache(
        "everything", chrome.test.callbackPass());
  },
  function testClearCookies() {
    chrome.experimental.clear.cookies(
        "everything", chrome.test.callbackPass());
  },
  function testClearHistory() {
    chrome.experimental.clear.history(
        "everything", chrome.test.callbackPass());
  },
  function testClearPasswords() {
    chrome.experimental.clear.passwords(
        "everything", chrome.test.callbackPass());
  },
  function testClearDownloads() {
    chrome.experimental.clear.downloads(
        "everything", chrome.test.callbackPass());
  },
  function testClearFormData() {
    chrome.experimental.clear.formData(
        "everything", chrome.test.callbackPass());
  }
]);
