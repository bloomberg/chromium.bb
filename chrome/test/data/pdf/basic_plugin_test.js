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
    chrome.test.assertEq(1066 + viewer.viewport.topToolbarHeight_,
                         sizer.offsetHeight);
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

  /**
   * Test that the filename is used as the title.pdf.
   */
  function testHasCorrectTitle() {
    chrome.test.assertEq('test.pdf', document.title);

    chrome.test.succeed();
  },

  /**
   * Test that the escape key gets propogated to the outer frame (via the
   * PDFScriptingAPI) in print preview.
   */
  function testEscKeyPropogationInPrintPreview() {
    viewer.isPrintPreview_ = true;
    scriptingAPI.setKeyEventCallback(chrome.test.callbackPass(function(e) {
      chrome.test.assertEq(27, e.keyCode);
    }));
    var e = document.createEvent('Event');
    e.initEvent('keydown');
    e.keyCode = 27;
    document.dispatchEvent(e);
  }
];

var scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCallback(function() {
  chrome.test.runTests(tests);
});
