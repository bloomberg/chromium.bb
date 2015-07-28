// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Standalone unit tests of the PDF Polymer elements.
 */
var tests = [
  /**
   * Test that viewer-page-selector reacts correctly to text entry. The page
   * selector validates that input is an integer, but does not check for
   * document bounds.
   */
  function testPageSelectorChange() {
    var selector =
        Polymer.Base.create('viewer-page-selector', {docLength: 1234});

    var input = selector.$.input;
    // Simulate entering text into `input` and pressing enter.
    function changeInput(newValue) {
      input.bindValue = newValue;
      input.dispatchEvent(new CustomEvent('change'));
    }

    var navigatedPages = [];
    selector.addEventListener('change-page', function(e) {
      navigatedPages.push(e.detail.page);
    });

    changeInput("1000");
    changeInput("1234");
    changeInput("abcd");
    changeInput("12pp");
    changeInput("3.14");

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
    chrome.test.assertEq('/ 1234', selector.$.pagelength.textContent);
    chrome.test.assertEq('2.4em', selector.$.pageselector.style.width);
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

    // Wait for the <template>s to be properly initialized.
    bookmarkContent.async(function() {
      var rootBookmarks =
          bookmarkContent.shadowRoot.querySelectorAll('viewer-bookmark');
      chrome.test.assertEq(1, rootBookmarks.length, "one root bookmark");
      var rootBookmark = rootBookmarks[0];

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

      var subBookmarkDiv =
          rootBookmark.shadowRoot.querySelector('.sub-bookmark');

      chrome.test.assertTrue(subBookmarkDiv.hidden);
      MockInteractions.tap(rootBookmark.$.expand);
      chrome.test.assertFalse(subBookmarkDiv.hidden);
      chrome.test.assertEq('hidden', subBookmarks[1].$.expand.style.visibility);

      chrome.test.succeed();
    });
  }
];

importTestHelpers().then(function() {
  chrome.test.runTests(tests);
});
