// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function logOnLinux(msg) {
  if (navigator.userAgent.search("X11; Linux") != -1) {
    console.log(msg);
  }
}

function closeEnough(actual, expected, maxAllowedDifference) {
  return Math.abs(actual - expected) <= maxAllowedDifference;
}

function boundsEqual(b1, b2, slop) {
  if (!b1 || !b2) {
    console.error("boundsEqual: got null bounds");
    window.close();
    return false;
  }
  if (!slop)
    slop = 0;
  return (closeEnough(b1.left, b2.left, slop) &&
          closeEnough(b1.top, b2.top, slop) &&
          closeEnough(b1.width, b2.width, slop) &&
          closeEnough(b1.height, b2.height, slop));
}

function waitForBounds(expectedBounds, callback) {
  var currentWindow = chrome.app.window.current();
  function checkBounds() {
    var bounds = currentWindow.getBounds();
    if (!bounds) {
      console.error("getBounds returned null!");
      window.close();
      return;
    }
    if (boundsEqual(bounds, expectedBounds, slop)) {
      logOnLinux("bounds equal - actual was " + JSON.stringify(bounds));
      currentWindow.onBoundsChanged.removeListener(checkBounds);
      callback();
    } else {
      logOnLinux("bounds unequal - actual was " + JSON.stringify(bounds));
    }
  }
  currentWindow.onBoundsChanged.addListener(checkBounds);
  checkBounds();
}

var slop = 0;

logOnLinux("sending ready");
chrome.test.sendMessage("ready", function(response) {
  slop = parseInt(response);
  logOnLinux("got slop of " + slop + ", waiting for first bounds");
  waitForBounds({left:100, top:200, width:300, height:400}, function() {
    logOnLinux("saw first bounds, now resizing");
    var newBounds = {left:50, top:100, width:150, height:200};
    chrome.app.window.current().setBounds(newBounds);
    waitForBounds(newBounds, function(){
      logOnLinux("saw second bounds, sending success message");
      chrome.test.sendMessage("success");
    });
  });
});
