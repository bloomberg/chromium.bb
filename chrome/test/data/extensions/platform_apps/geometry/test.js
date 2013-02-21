// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

var assertBoundsEqual = function(a, b) {
  chrome.test.assertEq(a.left, b.left, 'left');
  chrome.test.assertEq(a.top, b.top, 'top');
  chrome.test.assertEq(a.width, b.width, 'width');
  chrome.test.assertEq(a.height, b.height, 'height');
};

var testRestoreSize = function(id, frameType) {
  chrome.app.window.create('empty.html',
      { id: id,
        left: 113, top: 117, width: 314, height: 271,
        frame: frameType }, callbackPass(function(win) {
    var bounds = win.getBounds();

    assertBoundsEqual({ left:113, top:117, width:314, height:271 }, bounds);
    var newBounds = { left:447, top:440, width:647, height:504 };
    win.onBoundsChanged.addListener(callbackPass(boundsChanged));
    win.setBounds(newBounds);
    function boundsChanged() {
      assertBoundsEqual(newBounds, win.getBounds());
      win.close();
      chrome.app.window.create('empty.html',
          { id: id,
            left: 113, top: 117, width: 314, height: 271,
            frame: frameType }, callbackPass(function(win2) {
        assertBoundsEqual(newBounds, win2.getBounds());
      }));
    }
  }));
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([
    function testFramelessRestoreSize() {
      testRestoreSize('frameless', 'none');
    },
    function testFramedRestoreSize() {
      testRestoreSize('framed', 'chrome');
    },
  ]);
});
