// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  /**
   * Test that ToolbarManager.forceHideTopToolbar hides (or shows) the top
   * toolbar correctly for different mouse movements.
   */
  function testToolbarManagerForceHideTopToolbar() {
    var mockWindow = new MockWindow(1920, 1080);

    var toolbar = Polymer.Base.create('viewer-pdf-toolbar');
    var zoomToolbar = Polymer.Base.create('viewer-zoom-toolbar');
    var toolbarManager = new ToolbarManager(mockWindow, toolbar, zoomToolbar);

    // A cut-down version of MockInteractions.move, which is not exposed
    // publicly.
    var mouseMove = function(fromX, fromY, toX, toY, steps) {
      var dx = Math.round((toX - fromX) / steps);
      var dy = Math.round((toY - fromY) / steps);

      // Deliberate <= to ensure that an event is run for toX, toY
      for (var i = 0; i <= steps; i++) {
        var e = new MouseEvent('mousemove', {
          clientX: fromX,
          clientY: fromY,
          movementX: dx,
          movementY: dy
        });
        toolbarManager.showToolbarsForMouseMove(e);
        fromX += dx;
        fromY += dy;
      }
    }

    // Force hide the toolbar, then do a quick mousemove in the center of the
    // window. Top toolbar should not show.
    toolbarManager.forceHideTopToolbar();
    chrome.test.assertFalse(toolbar.opened);
    mouseMove(1900, 1000, 100, 1000, 2);
    chrome.test.assertFalse(toolbar.opened);
    // Move back into the zoom toolbar again. The top toolbar should still not
    // show.
    mouseMove(100, 1000, 1900, 1000, 2);
    chrome.test.assertFalse(toolbar.opened);

    // Hide the toolbar, wait for the timeout to expire, then move the mouse
    // quickly. The top toolbar should show.
    toolbarManager.forceHideTopToolbar();
    chrome.test.assertFalse(toolbar.opened);
    // Manually expire the timeout. This is the same as waiting 1 second.
    mockWindow.runTimeout();
    mouseMove(1900, 1000, 100, 1000, 2);
    chrome.test.assertTrue(toolbar.opened);

    // Force hide the toolbar, then move the mouse to the top of the screen. The
    // top toolbar should show.
    toolbarManager.forceHideTopToolbar();
    chrome.test.assertFalse(toolbar.opened);
    mouseMove(1900, 1000, 1000, 30, 5);
    chrome.test.assertTrue(toolbar.opened);

    chrome.test.succeed();
  },

  /**
   * Test that changes to window height bubble down to dropdowns correctly.
   */
  function testToolbarManagerResizeDropdown() {
    var mockWindow = new MockWindow(1920, 1080);
    var mockZoomToolbar = {
      clientHeight: 400
    };
    var toolbar = document.getElementById('material-toolbar');
    var bookmarksDropdown = toolbar.$.bookmarks;

    var toolbarManager =
        new ToolbarManager(mockWindow, toolbar, mockZoomToolbar);

    chrome.test.assertTrue(bookmarksDropdown.lowerBound == 680);

    mockWindow.setSize(1920, 480);
    chrome.test.assertTrue(bookmarksDropdown.lowerBound == 80);

    chrome.test.succeed();
  },
];

importTestHelpers().then(function() {
  chrome.test.runTests(tests);
});
