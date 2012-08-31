// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('page1.html',
    {'id': 'test', 'defaultLeft': 113, 'defaultTop': 117,
     'defaultWidth': 314, 'defaultHeight': 271, 'frame': 'none'},
      function () {});

  chrome.test.sendMessage('WaitForPage2', function(response) {
    chrome.app.window.create('page2.html',
       {'id': 'test', 'defaultLeft': 113, 'defaultTop': 117,
        'defaultWidth': 314, 'defaultHeight': 271, 'frame': 'none'},
        function () {});
  });

  chrome.test.sendMessage('WaitForPage3', function(response) {
    chrome.app.window.create('page3.html',
      {'id': 'test', 'left': 201, 'top': 220,
       'defaultWidth': 314, 'defaultHeight': 271, 'frame': 'none'},
        function () {});
  });

});
