// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupListener() {
  chrome.syncFileSystem.onFileSynced.addListener(fileSyncEventReceived);
  chrome.syncFileSystem.requestFileSystem('drive', function() {});
}

function fileSyncEventReceived(file_entry_path, sync_operation_result) {
  chrome.test.assertEq("foo", file_entry_path);
  chrome.test.assertEq("added", sync_operation_result);
  chrome.test.succeed();
}

chrome.test.runTests([
  setupListener
]);
