// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getEnabled() {
    chrome.hotwordPrivate.getStatus(
        chrome.test.callbackPass(function(result) {
          chrome.test.assertFalse(result.enabledSet);
        }));

    chrome.hotwordPrivate.setEnabled(true, function() {});
    chrome.hotwordPrivate.getStatus(
        chrome.test.callbackPass(function(result) {
          chrome.test.assertTrue(result.enabled);
        }));
    chrome.hotwordPrivate.getStatus(
        chrome.test.callbackPass(function(result) {
          chrome.test.assertTrue(result.enabledSet);
        }));

    chrome.hotwordPrivate.setEnabled(false, function() {});
    chrome.hotwordPrivate.getStatus(
        chrome.test.callbackPass(function(result) {
          chrome.test.assertFalse(result.enabled);
        }));
    chrome.hotwordPrivate.getStatus(
        chrome.test.callbackPass(function(result) {
          chrome.test.assertTrue(result.enabledSet);
        }));
  }
]);
