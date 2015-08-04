// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var scriptingAPI;

/**
 * These tests require that the PDF plugin be available to run correctly.
 */
var tests = [
  /**
   * Test that the page is sized to the size of the document.
   */
  function testPageSize() {
    // Verify that the initial zoom is less than or equal to 100%.
    chrome.test.assertTrue(viewer.viewport.zoom <= 1);

    viewer.viewport.setZoom(1);
    var sizer = document.getElementById('sizer');
    chrome.test.assertEq(826, sizer.offsetWidth);
    chrome.test.assertEq(1066, sizer.offsetHeight);
    chrome.test.succeed();
  },

  function testAccessibility() {
    scriptingAPI.getAccessibilityJSON(chrome.test.callbackPass(function(json) {
      var dict = JSON.parse(json);
      chrome.test.assertEq(true, dict.copyable);
      chrome.test.assertEq(true, dict.loaded);
      chrome.test.assertEq(1, dict.numberOfPages);
    }));
  },

  function testAccessibilityWithPage() {
    scriptingAPI.getAccessibilityJSON(chrome.test.callbackPass(function(json) {
      var dict = JSON.parse(json);
      chrome.test.assertEq(612, dict.width);
      chrome.test.assertEq(792, dict.height);
      chrome.test.assertEq(1.0, dict.textBox[0].fontSize);
      chrome.test.assertEq('text', dict.textBox[0].textNodes[0].type);
      chrome.test.assertEq('this is some text',
                           dict.textBox[0].textNodes[0].text);
      chrome.test.assertEq(1.0, dict.textBox[1].fontSize);
      chrome.test.assertEq('text', dict.textBox[1].textNodes[0].type);
      chrome.test.assertEq('some more text',
                           dict.textBox[1].textNodes[0].text);
    }), 0);
  },

  function testGetSelectedText() {
    var client = new PDFScriptingAPI(window, window);
    client.selectAll();
    client.getSelectedText(chrome.test.callbackPass(function(selectedText) {
      chrome.test.assertEq('this is some text\nsome more text', selectedText);
    }));
  },
];

var scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCallback(function() {
  chrome.test.runTests(tests);
});
