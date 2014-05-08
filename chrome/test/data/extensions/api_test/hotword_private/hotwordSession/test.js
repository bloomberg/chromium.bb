// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function hotwordSessionTest() {
    chrome.hotwordPrivate.onHotwordSessionRequested.addListener(function () {
      chrome.hotwordPrivate.setHotwordSessionState(true, function() {
        chrome.hotwordPrivate.notifyHotwordRecognition('search', function() {
          chrome.hotwordPrivate.onHotwordSessionStopped.addListener(
              function() {
                // Calling setHotwordSessionState and
                // notifyHotwordRecognition after stopped should be ignored.
                chrome.hotwordPrivate.setHotwordSessionState(false, function() {
                  chrome.hotwordPrivate.notifyHotwordRecognition(
                      'search', function() {
                        chrome.test.sendMessage("stopped");
                        chrome.test.succeed();
                      });
                });
              });
          chrome.test.sendMessage("stopReady");
        });
      });
    });
    chrome.test.sendMessage("ready");
  }
]);
