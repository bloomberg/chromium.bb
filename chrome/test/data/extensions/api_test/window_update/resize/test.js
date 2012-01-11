// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;

var widthDelta = 10;
var heightDelta = 20;

var expectedWidth;
var expectedHeight;

function finishTest(currentWindow) {
  chrome.test.assertEq(expectedWidth, currentWindow.width);
  chrome.test.assertEq(expectedHeight, currentWindow.height);

  chrome.windows.remove(currentWindow.id, pass());
}

function changeWidthAndHeight(currentWindow) {
  chrome.test.assertEq(expectedWidth, currentWindow.width);
  chrome.test.assertEq(expectedHeight, currentWindow.height);

  expectedWidth = currentWindow.width + widthDelta;
  expectedHeight = currentWindow.height + heightDelta;
  chrome.windows.update(
    currentWindow.id, { 'width': expectedWidth , 'height': expectedHeight},
    pass(finishTest)
  );
}

function changeHeight(currentWindow) {
  chrome.test.assertEq(expectedWidth, currentWindow.width);
  chrome.test.assertEq(expectedHeight, currentWindow.height);

  expectedWidth = currentWindow.width;
  expectedHeight = currentWindow.height + heightDelta;
  chrome.windows.update(
    currentWindow.id, { 'height': expectedHeight },
    pass(changeWidthAndHeight)
  );
}

function changeWidth(currentWindow) {
  expectedWidth = currentWindow.width + widthDelta;
  expectedHeight = currentWindow.height;
  chrome.windows.update(
    currentWindow.id, { 'width': expectedWidth },
    pass(changeHeight)
  );
}

chrome.test.runTests([
  function testResizeNormal() {
    chrome.windows.create(
        { 'url': 'blank.html', 'width': 500, 'height': 600, 'type': 'normal' },
        pass(changeWidth));
  },
  function testResizePopup() {
    chrome.windows.create(
        { 'url': 'blank.html', 'width': 300, 'height': 500, 'type': 'popup' },
        pass(changeWidth));
  },
  function testResizePanel() {
    chrome.windows.create(
        { 'url': 'blank.html', 'width': 150, 'height': 200, 'type': 'panel' },
        pass(changeWidth));
  },
]);
