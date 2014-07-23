// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  runAllTests('http://www.test.com:' + config.testServer.port);
});

function runAllTests(testServerUrl) {
  chrome.test.runTests([
    function dumpLogsTest() {
      chrome.logPrivate.dumpLogs(function(fileEntry) {
        if (!fileEntry.name.match(/\.tar\.gz$/g)) {
          chrome.test.fail('Invalid file type: '+ fileEntry.name);
          return;
        }

        fileEntry.file(function(file) {
            var reader = new FileReader();
            reader.onloadend = function(e) {
              if (this.result.byteLength > 0)
                chrome.test.succeed();
              else
                chrome.test.fail('Invalid log file content');
            };
            reader.readAsArrayBuffer(file);
          },
          function() {
            chrome.test.fail('File reading error');
          });
      });
    },
    function captureNetworkEvents() {
      chrome.logPrivate.onCapturedEvents.addListener(function(entries) {
        for (var i = 0; i < entries.length; i++) {
          // Find our XHR request from below:
          if (entries[i].params && entries[i].params.url &&
                  entries[i].params.url.match(/www\.test\.com/g)) {
            chrome.logPrivate.stopEventRecorder('network', function() {
              chrome.test.succeed();
            });
          }
        }
      });
      chrome.logPrivate.startEventRecorder('network', 'capture', function() {
        var req = new XMLHttpRequest();
        req.onreadystatechange = function() {}
        req.open('GET', testServerUrl, true);
        req.send();
      });
  }
  ]);
}
