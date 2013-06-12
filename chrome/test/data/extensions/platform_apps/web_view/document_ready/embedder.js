// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.addEventListener('DOMContentLoaded', function(e) {
  var webview = document.querySelector('webview');
  webview.addEventListener('loadstop', function(e) {
    // Note that we are relying on .partition property to read the partition.
    // The other way would be to read this value from BrowserPluginGuest in cpp
    // code, which is slower and requires mode code (hurts readability of the
    // test).
    var partitionName = webview.partition;
    chrome.test.runTests([
      function checkPartition() {
        if (partitionName == 'persist:test-partition') {
          chrome.test.succeed();
        } else {
          chrome.test.fail();
        }
      }
    ]);
  });
  webview.partition = 'persist:test-partition';
  webview.setAttribute('src', 'data:text/html,<body>Test</body>');
});
