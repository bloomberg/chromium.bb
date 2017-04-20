// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testHitTestInDesktop() {
    var url = 'data:text/html,<!doctype html>' +
        encodeURI('<button>Click Me</button>');
    var didHitTest = false;
    chrome.automation.getDesktop(function(desktop) {
      chrome.tabs.create({url: url});

      desktop.addEventListener('loadComplete', function(event) {
        if (didHitTest)
          return;
        if (event.target.url.indexOf('data:') >= 0) {
          var button = desktop.find({ attributes: { name: 'Click Me' } });
          if (button) {
            didHitTest = true;
            button.addEventListener(EventType.ALERT, function() {
              chrome.test.succeed();
            }, true);
            var cx = Math.floor(
                button.location.left + button.location.width / 2);
            var cy = Math.floor(
                button.location.top + button.location.height / 2);
            desktop.hitTest(cx, cy, EventType.ALERT);
          }
        }
      }, false);
    });
  },
];

chrome.test.runTests(allTests);
