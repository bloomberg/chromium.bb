// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function managedChangeEvents() {
    // Verify initial values.
    chrome.storage.managed.get(
        chrome.test.callbackPass(function(results) {
          chrome.test.assertEq({
            'constant-policy': 'aaa',
            'changes-policy': 'bbb',
            'deleted-policy': 'ccc'
          }, results);

          // Now start listening for changes.
          // chrome.test.listenOnce() adds and removes the listener to the
          // given event, and only lets the test complete after receiving the
          // event.
          chrome.test.listenOnce(
              chrome.storage.onChanged,
              function(changes, namespace) {
                chrome.test.assertEq('managed', namespace);
                chrome.test.assertEq({
                    'changes-policy': {
                      'oldValue': 'bbb',
                      'newValue': 'ddd'
                    },
                    'deleted-policy': {
                      'oldValue': 'ccc'
                    },
                    'new-policy': {
                      'newValue': 'eee'
                    }
                }, changes);
              });

          // Signal to the browser that we're listening.
          chrome.test.sendMessage('ready');
        }));
  }
]);
