// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  function checkBounds() {
    var bounds = chrome.app.window.current().getBounds();
    if (!bounds) {
      console.error("getBounds returned null!");
      window.close();
      return;
    }
    if (boundsEqual(bounds, expectedBounds, slop)) {
      callback();
    } else {
      // TODO(asargent) - change this to just listen for the bounds change event
      // once that's supported.
      setTimeout(checkBounds, 50);
    }
  }
  checkBounds();
}

var slop = 0;

chrome.test.sendMessage("ready", function(response) {
  slop = parseInt(response);
  waitForBounds({left:100, top:200, width:300, height:400}, function() {
    var newBounds = {left:50, top:100, width:150, height:200};
    chrome.app.window.current().setBounds(newBounds);
    waitForBounds(newBounds, function(){
      chrome.test.sendMessage("success");
    });
  });
});
