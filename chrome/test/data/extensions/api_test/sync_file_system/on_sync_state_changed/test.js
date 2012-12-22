// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupListener() {
  chrome.syncFileSystem.onSyncStateChanged.addListener(checkEventReceived);
  chrome.syncFileSystem.requestFileSystem('drive', function() {});
}

function checkEventReceived(syncState) {
  chrome.test.assertEq("drive", syncState.service_name);
  chrome.test.assertEq("running", syncState.state);
  chrome.test.assertEq("Test event description.", syncState.description);
  chrome.test.succeed();
}

chrome.test.runTests([
  setupListener
]);
