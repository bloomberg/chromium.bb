// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testForUrl(fileUrl) {
  chrome.fileBrowserPrivate.getFileTasks([fileUrl], [], function(tasks) {
    if (!tasks || !tasks.length) {
      chrome.test.fail('No tasks registered');
      return;
    }
    console.log('Tasks: ' + tasks.length);
    chrome.fileBrowserPrivate.executeTask(tasks[0].taskId, [fileUrl]);
  });
};

chrome.test.runTests([function() {
  var hash = window.location.hash.toString();

  if (!hash.length) {
    chrome.test.fail('No fileUrl given to test');
    return;
  }
  var fileUrl = hash.substring(1);

  // The local filesystem is not explicitly used, but this test still needs to
  // request it; it configures local access permissions.
  chrome.fileBrowserPrivate.requestLocalFileSystem(chrome.test.callbackPass(
      function(fs) {
    testForUrl(fileUrl);
  }));
}]);
