// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that we can call the tabs API. We don't care what the result is; tabs
// are tested elsewhere. We just care that we can call it without a
// permissions error.
chrome.windows.getAll(null, function() {
  chrome.test.assertNoLastError();
  chrome.extension.getBackgroundPage().onNotificationDone();
});
