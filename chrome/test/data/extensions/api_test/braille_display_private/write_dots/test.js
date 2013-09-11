// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test for brailleDisplayPrivate.writeDots.
// browser_tests.exe --gtest_filter="BrailleDisplayPrivateApiTest.*"

var pass = chrome.test.callbackPass;

function createBuffer(size, element) {
  var buf = new Uint8Array(size);
  for (var i = 0; i < size; ++i) {
    buf[i] = element;
  }
  return buf.buffer;
}

chrome.test.runTests([
  function testWriteEmptyCells() {
    chrome.brailleDisplayPrivate.getDisplayState(pass(
        function(state) {
          chrome.test.assertTrue(state.available, "Display not available");
          chrome.test.assertEq(11, state.textCellCount);
          chrome.brailleDisplayPrivate.writeDots(new ArrayBuffer(0));
          chrome.brailleDisplayPrivate.getDisplayState(pass());
        }));
  },

  function testWriteOversizedCells() {
    chrome.brailleDisplayPrivate.getDisplayState(pass(
        function(state) {
          chrome.test.assertTrue(state.available, "Display not available");
          chrome.test.assertEq(11, state.textCellCount);
          chrome.brailleDisplayPrivate.writeDots(
              createBuffer(state.textCellCount + 1, 1));
          chrome.brailleDisplayPrivate.writeDots(
              createBuffer(1000000, 2));
          chrome.brailleDisplayPrivate.getDisplayState(pass());
        }));
  }
]);
