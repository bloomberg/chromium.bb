// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

function testAlwaysOnTop(testId, initValue, setOption) {
  var options = { id: testId };
  if (setOption)
    options.alwaysOnTop = initValue;

  chrome.app.window.create('index.html',
                           options,
                           callbackPass(function(win) {
    // Check that isAlwaysOnTop() returns false, since this test is being run
    // in Stable.
    chrome.test.assertEq(false, win.isAlwaysOnTop());

    // Toggle the current value.
    // TODO(tmdiep): The new value of isAlwaysOnTop() should be checked
    // asynchronously, but we need a non-flaky way to do this.
    win.setAlwaysOnTop(!initValue);

    win.contentWindow.close();
  }));
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([

    // Window is created with always on top enabled.
    function testAlwaysOnTopInitTrue() {
      testAlwaysOnTop('testAlwaysOnTopInitTrue', true, true);
    },

    // Window is created with always on top explicitly disabled.
    function testAlwaysOnTopInitFalse() {
      testAlwaysOnTop('testAlwaysOnTopInitFalse', false, true);
    },

    // Window is created with option not explicitly set.
    function testAlwaysOnTopNoInit() {
      testAlwaysOnTop('testAlwaysOnTopNoInit', false, false);
    }

  ]);
});
