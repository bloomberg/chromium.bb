// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.sendMessage('Launched');

  var win1;
  chrome.app.window.create('empty.html',
      { id: 'test',
        left: 113, top: 117, width: 314, height: 271,
        frame: 'none' }, function(win) {
    win1 = win;

    var bounds = win.getBounds();

    // TODO(jeremya): convert to use getBounds() once
    // https://codereview.chromium.org/11369039/ lands.
    chrome.test.assertEq(113, bounds.left);
    chrome.test.assertEq(117, bounds.top);
    chrome.test.assertEq(314, bounds.width);
    chrome.test.assertEq(271, bounds.height);
    chrome.test.sendMessage('Done1');
  });

  chrome.test.sendMessage('WaitForPage2', function(response) {
    win1.close();
    chrome.app.window.create('empty.html',
        { id: 'test',
          left: 113, top: 117, width: 314, height: 271,
          frame: 'none' }, function(win) {
      var bounds = win.getBounds();

      // TODO(jeremya): convert to use getBounds() once
      // https://codereview.chromium.org/11369039/ lands.
      chrome.test.assertEq(137, bounds.left);
      chrome.test.assertEq(143, bounds.top);
      chrome.test.assertEq(203, bounds.width);
      chrome.test.assertEq(187, bounds.height);
      chrome.test.sendMessage('Done2');
      win.close()
      window.close();
    });
  });
});
