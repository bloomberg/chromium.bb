// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var initialUserGesture = chrome.test.isProcessingUserGesture();

chrome.browserAction.onClicked.addListener(() => {
  chrome.test.assertFalse(initialUserGesture);

  // We should be running with a user gesture.
  // TODO(lazyboy): isProcessingUserGesture() only performs renderer level
  // checks, also call an actual API that requires gesture.
  chrome.test.assertTrue(chrome.test.isProcessingUserGesture());
  // Call an API so we can check gesture state in the callback.
  chrome.tabs.create({url: chrome.runtime.getURL('page.html')}, () => {
    chrome.test.assertNoLastError();
    chrome.test.assertFalse(chrome.test.isProcessingUserGesture());
    chrome.test.notifyPass();
  });
});

chrome.test.sendMessage('ready');
