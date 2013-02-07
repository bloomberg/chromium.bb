// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupListener() {
  chrome.syncFileSystem.onFileStatusChanged.addListener(fileInfoReceived);
  chrome.syncFileSystem.requestFileSystem(function() {});
}

function fileInfoReceived(file_entry, sync_operation_status) {
  chrome.test.assertEq("foo.txt", file_entry.name);
  chrome.test.assertEq("/foo.txt", file_entry.fullPath);
  chrome.test.assertTrue(file_entry.isFile);
  chrome.test.assertFalse(file_entry.isDirectory);
  chrome.test.assertEq("added", sync_operation_status);
  chrome.test.succeed();
}

chrome.test.runTests([
  setupListener
]);
