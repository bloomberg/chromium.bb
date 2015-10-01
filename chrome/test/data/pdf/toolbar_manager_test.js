// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A cut-down version of MockInteractions.move, which is not exposed
// publicly.
function getMouseMoveEvents(fromX, fromY, toX, toY, steps) {
  var dx = Math.round((toX - fromX) / steps);
  var dy = Math.round((toY - fromY) / steps);
  var events = [];

  // Deliberate <= to ensure that an event is run for toX, toY
  for (var i = 0; i <= steps; i++) {
    var e = new MouseEvent('mousemove', {
      clientX: fromX,
      clientY: fromY,
      movementX: dx,
      movementY: dy
    });
    events.push(e);
    fromX += dx;
    fromY += dy;
  }
  return events;
}

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

    var mouseMove = function(fromX, fromY, toX, toY, steps) {
      getMouseMoveEvents(fromX, fromY, toX, toY, steps).forEach(function(e) {
        toolbarManager.showToolbarsForMouseMove(e);
      });
    };

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

    chrome.test.assertEq(680, bookmarksDropdown.lowerBound);

    mockWindow.setSize(1920, 480);
    chrome.test.assertEq(80, bookmarksDropdown.lowerBound);

    chrome.test.succeed();
  },

  /**
   * Test that the toolbar will not be hidden when navigating with the tab key.
   */
  function testToolbarKeyboardNavigation() {
    var mockWindow = new MockWindow(1920, 1080);
    var toolbar =
        Polymer.Base.create('viewer-pdf-toolbar', {loadProgress: 100});
    var zoomToolbar = Polymer.Base.create('viewer-zoom-toolbar');
    var toolbarManager = new ToolbarManager(mockWindow, toolbar, zoomToolbar);

    var mouseMove = function(fromX, fromY, toX, toY, steps) {
      getMouseMoveEvents(fromX, fromY, toX, toY, steps).forEach(function(e) {
        toolbarManager.showToolbarsForMouseMove(e);
      });
    };

    // Move the mouse and then hit tab -> Toolbars stay open.
    mouseMove(200, 200, 800, 800, 5);
    toolbarManager.showToolbarsForKeyboardNavigation();
    chrome.test.assertTrue(toolbar.opened);
    mockWindow.runTimeout();
    chrome.test.assertTrue(toolbar.opened);

    // Hit escape -> Toolbars close.
    toolbarManager.hideSingleToolbarLayer();
    chrome.test.assertFalse(toolbar.opened);

    // Show toolbars, use mouse, run timeout -> Toolbars close.
    toolbarManager.showToolbarsForKeyboardNavigation();
    mouseMove(200, 200, 800, 800, 5);
    chrome.test.assertTrue(toolbar.opened);
    mockWindow.runTimeout();
    chrome.test.assertFalse(toolbar.opened);

    chrome.test.succeed();
  }
];

importTestHelpers().then(function() {
  chrome.test.runTests(tests);
});
