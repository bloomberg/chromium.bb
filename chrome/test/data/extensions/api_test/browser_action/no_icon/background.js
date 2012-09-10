// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var canvas = document.getElementById("canvas").getContext('2d').
    getImageData(0, 0, 19, 19);
var canvasHD = document.getElementById("canvas").getContext('2d').
    getImageData(0, 0, 38, 38);

var setIconParamQueue = [
  {imageData: canvas},
  {path: 'icon.png'},
  {imageData: {'19': canvas, '38': canvasHD}},
  {path: {'19': 'icon.png', '38': 'icon.png'}},
  {imageData: {'19': canvas}},
  {path: {'19': 'icon.png'}},
  {imageData: {'38': canvasHD}},
  {imageData: {}},
  {path: {}},
];

// Called when the user clicks on the browser action.
chrome.browserAction.onClicked.addListener(function(windowId) {
  if (setIconParamQueue.length == 0) {
    chrome.test.notifyFail("Queue of params for test cases unexpectedly empty");
    return;
  }

  try {
    chrome.browserAction.setIcon(setIconParamQueue.shift(), function() {
      chrome.test.notifyPass();});
  } catch (error) {
    console.log(error.message);
    chrome.test.notifyFail(error.message);
  }
});

chrome.test.notifyPass();
