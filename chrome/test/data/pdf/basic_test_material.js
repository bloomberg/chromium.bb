// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  /**
   * Test that some key elements exist and that they have the appropriate
   * constructor name. This verifies that polymer is working correctly.
   */
  function testHasElements() {
    var elementNames = [
      'viewer-pdf-toolbar',
      'viewer-zoom-toolbar',
      'viewer-password-screen',
      'viewer-error-screen'
    ];
    for (var i = 0; i < elementNames.length; i++) {
      var elements = document.querySelectorAll(elementNames[i]);
      chrome.test.assertEq(1, elements.length);
      var element = elements[0];
      chrome.test.assertTrue(
          String(element.constructor).indexOf(elementNames[i]) != -1);
    }
    chrome.test.succeed();
  },

  /**
   * Test that the plugin element exists and is navigated to the correct URL.
   */
  function testPluginElement() {
    var plugin = document.getElementById('plugin');
    chrome.test.assertEq('embed', plugin.localName);

    chrome.test.assertTrue(
        plugin.getAttribute('src').indexOf('/pdf/test.pdf') != -1);
    chrome.test.succeed();
  },

  /**
   * Test that shouldIgnoreKeyEvents correctly searches through the shadow DOM
   * to find input fields.
   */
  function testIgnoreKeyEvents() {
    // Test that the traversal through the shadow DOM works correctly.
    var toolbar = document.getElementById('material-toolbar');
    toolbar.$.pageselector.$.input.focus();
    chrome.test.assertTrue(shouldIgnoreKeyEvents(toolbar));

    // Test case where the active element has a shadow root of its own.
    toolbar.$.buttons.children[1].focus();
    chrome.test.assertFalse(shouldIgnoreKeyEvents(toolbar));

    chrome.test.assertFalse(
        shouldIgnoreKeyEvents(document.getElementById('plugin')));

    chrome.test.succeed();
  },

  /**
   * Test that changes to window height bubble down to dropdowns correctly.
   */
  function testUiManagerResizeDropdown() {
    var mockWindow = new MockWindow(1920, 1080);
    var mockZoomToolbar = {
      clientHeight: 400
    };
    var toolbar = document.getElementById('material-toolbar');
    var bookmarksDropdown = toolbar.$.bookmarks;

    var uiManager = new UiManager(mockWindow, toolbar, mockZoomToolbar);

    chrome.test.assertTrue(bookmarksDropdown.lowerBound == 680);

    mockWindow.setSize(1920, 480);
    chrome.test.assertTrue(bookmarksDropdown.lowerBound == 80);

    chrome.test.succeed();
  }
];

var scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCallback(function() {
  chrome.test.runTests(tests);
});
