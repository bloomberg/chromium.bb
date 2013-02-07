// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupListener() {
  chrome.syncFileSystem.onFileStatusChanged.addListener(fileInfoReceived);
  chrome.syncFileSystem.requestFileSystem(function() {});
}

// Confirm contents of FileEntry, should still be valid for deleted file.
function fileInfoReceived(file_entry, sync_operation_status) {
  chrome.test.assertEq("foo.txt", file_entry.name);
  chrome.test.assertEq("/foo.txt", file_entry.fullPath);
  chrome.test.assertTrue(file_entry.isFile);
  chrome.test.assertFalse(file_entry.isDirectory);
  chrome.test.assertEq("deleted", sync_operation_status);

  // Try to open file using FileEntry, should fail cause file was deleted.
  file_entry.file(getFileObjectSucceeded, getFileObjectFailed);
}

function getFileObjectFailed(e) {
  chrome.test.assertEq(FileError.NOT_FOUND_ERR, e.code);
  chrome.test.succeed();
}

function getFileObjectSucceeded(file_object) {
  chrome.test.fail("Synchronized file deleted, FileEntry.file() should fail.");
}

chrome.test.runTests([
  setupListener
]);
