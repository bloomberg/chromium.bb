// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Standalone unit tests of the PDF Polymer elements.
 */
var tests = [
  /**
   * Test that viewer-page-selector reacts correctly to text entry. The page
   * selector validates that input is an integer, and does not allow navigation
   * past document bounds.
   */
  function testPageSelectorChange() {
    var selector =
        Polymer.Base.create('viewer-page-selector', {docLength: 1234});

    var input = selector.$.input;
    // Simulate entering text into `input` and pressing enter.
    function changeInput(newValue) {
      input.value = newValue;
      input.dispatchEvent(new CustomEvent('change'));
    }

    var navigatedPages = [];
    selector.addEventListener('change-page', function(e) {
      navigatedPages.push(e.detail.page);
      // A change-page handler is expected to set the pageNo to the new value.
      selector.pageNo = e.detail.page + 1;
    });

    changeInput("1000");
    changeInput("1234");
    changeInput("abcd");
    changeInput("12pp");
    changeInput("3.14");
    changeInput("3000");

    chrome.test.assertEq(4, navigatedPages.length);
    // The event page number is 0-based.
    chrome.test.assertEq(999, navigatedPages[0]);
    chrome.test.assertEq(1233, navigatedPages[1]);
    chrome.test.assertEq(11, navigatedPages[2]);
    chrome.test.assertEq(2, navigatedPages[3]);

    chrome.test.succeed();
  },

  /**
   * Test that viewer-page-selector changes in response to setting docLength.
   */
  function testPageSelectorDocLength() {
    var selector =
        Polymer.Base.create('viewer-page-selector', {docLength: 1234});
    chrome.test.assertEq('1234', selector.$.pagelength.textContent);
    chrome.test.assertEq('4ch', selector.$.pageselector.style.width);
    chrome.test.succeed();
  },

  /**
   * Test that clicking the dropdown icon opens/closes the dropdown.
   */
  function testToolbarDropdownShowHide() {
    var dropdown = Polymer.Base.create('viewer-toolbar-dropdown', {
      header: 'Test Menu',
      closedIcon: 'closedIcon',
      openIcon: 'openIcon'
    });

    chrome.test.assertFalse(dropdown.dropdownOpen);
    chrome.test.assertEq('closedIcon', dropdown.dropdownIcon);

    MockInteractions.tap(dropdown.$.icon);

    chrome.test.assertTrue(dropdown.dropdownOpen);
    chrome.test.assertEq('openIcon', dropdown.dropdownIcon);

    MockInteractions.tap(dropdown.$.icon);

    chrome.test.assertFalse(dropdown.dropdownOpen);

    chrome.test.succeed();
  },

  /**
   * Test that viewer-bookmarks-content creates a bookmark tree with the correct
   * structure and behaviour.
   */
  function testBookmarkStructure() {
    var bookmarkContent = Polymer.Base.create('viewer-bookmarks-content', {
      bookmarks: [{
        title: 'Test 1',
        page: 1,
        children: [{
          title: 'Test 1a',
          page: 2,
          children: []
        },
        {
          title: 'Test 1b',
          page: 3,
          children: []
        }]
      }],
      depth: 1
    });

    // Force templates to render.
    Polymer.dom.flush();

    var rootBookmarks =
        bookmarkContent.shadowRoot.querySelectorAll('viewer-bookmark');
    chrome.test.assertEq(1, rootBookmarks.length, "one root bookmark");
    var rootBookmark = rootBookmarks[0];
    MockInteractions.tap(rootBookmark.$.expand);

    Polymer.dom.flush();

    var subBookmarks =
        rootBookmark.shadowRoot.querySelectorAll('viewer-bookmark');
    chrome.test.assertEq(2, subBookmarks.length, "two sub bookmarks");
    chrome.test.assertEq(1, subBookmarks[1].depth,
                           "sub bookmark depth correct");

    var lastPageChange;
    rootBookmark.addEventListener('change-page', function(e) {
      lastPageChange = e.detail.page;
    });

    MockInteractions.tap(rootBookmark.$.item);
    chrome.test.assertEq(1, lastPageChange);

    MockInteractions.tap(subBookmarks[1].$.item);
    chrome.test.assertEq(3, lastPageChange);

    chrome.test.succeed();
  },

  /**
   * Test that the zoom toolbar toggles between showing the fit-to-page and
   * fit-to-width buttons.
   */
  function testZoomToolbarToggle() {
    var zoomToolbar = Polymer.Base.create('viewer-zoom-toolbar', {});
    var fitButton = zoomToolbar.$['fit-button'];
    var fab = fitButton.$['button'];

    var fitWidthIcon = 'fullscreen';
    var fitPageIcon = 'fullscreen-exit';

    var lastEvent = null;
    var logEvent = function(e) {
      lastEvent = e.type;
    }
    var assertEvent = function(type) {
      chrome.test.assertEq(type, lastEvent);
      lastEvent = null;
    }
    zoomToolbar.addEventListener('fit-to-width', logEvent);
    zoomToolbar.addEventListener('fit-to-page', logEvent);

    // Initial: Show fit-to-page.
    // TODO(tsergeant): This assertion attempts to be resilient to iconset
    // changes. A better solution is something like
    // https://github.com/PolymerElements/iron-icon/issues/68.
    chrome.test.assertTrue(fab.icon.endsWith(fitPageIcon));

    // Tap 1: Fire fit-to-page, show fit-to-width.
    MockInteractions.tap(fab);
    assertEvent('fit-to-page');
    chrome.test.assertTrue(fab.icon.endsWith(fitWidthIcon));

    // Tap 2: Fire fit-to-width, show fit-to-page.
    MockInteractions.tap(fab);
    assertEvent('fit-to-width');
    chrome.test.assertTrue(fab.icon.endsWith(fitPageIcon));

    // Tap 3: Fire fit-to-page again.
    MockInteractions.tap(fab);
    assertEvent('fit-to-page');

    // Do the same as above, but with fitToggleFromHotKey().
    zoomToolbar.fitToggleFromHotKey();
    assertEvent('fit-to-width');
    zoomToolbar.fitToggleFromHotKey();
    assertEvent('fit-to-page');
    zoomToolbar.fitToggleFromHotKey();
    assertEvent('fit-to-width');

    // Tap 4: Fire fit-to-page again.
    MockInteractions.tap(fab);
    assertEvent('fit-to-page');

    chrome.test.succeed();
  }
];

importTestHelpers().then(function() {
  chrome.test.runTests(tests);
});
