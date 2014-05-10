// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * These tests require that the PDF plugin be available to run correctly.
 */
var tests = [
  /**
   * Test that the page is sized to the size of the document.
   */
  function testPageSize() {
    // Verify that the initial zoom is 100%.
    chrome.test.assertEq(1, viewer.viewport.zoom);

    var sizer = document.getElementById('sizer');
    chrome.test.assertEq(826, sizer.offsetWidth);
    chrome.test.assertEq(1066, sizer.offsetHeight);
    chrome.test.succeed();
  },
];

window.addEventListener('pdfload', function() {
  chrome.test.runTests(tests);
});
