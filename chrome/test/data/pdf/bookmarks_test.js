// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  /**
   * Test that the correct bookmarks were loaded for test-bookmarks.pdf.
   */
  function testHasCorrectBookmarks() {
    var bookmarks = viewer.bookmarks;

    // Load all relevant bookmarks.
    chrome.test.assertEq(3, bookmarks.length);
    var firstBookmark = bookmarks[0];
    var secondBookmark = bookmarks[1];
    var uriBookmark = bookmarks[2];
    chrome.test.assertEq(1, firstBookmark.children.length);
    chrome.test.assertEq(0, secondBookmark.children.length);
    var firstNestedBookmark = firstBookmark.children[0];

    // Check titles.
    chrome.test.assertEq('First Section',
                         firstBookmark.title);
    chrome.test.assertEq('First Subsection',
                         firstNestedBookmark.title);
    chrome.test.assertEq('Second Section',
                         secondBookmark.title);
    chrome.test.assertEq('URI Bookmark', uriBookmark.title);

    // Check pages.
    chrome.test.assertEq(0, firstBookmark.page);
    chrome.test.assertEq(1, firstNestedBookmark.page);
    chrome.test.assertEq(2, secondBookmark.page);
    chrome.test.assertEq(undefined, uriBookmark.page);

    chrome.test.assertEq('http://www.chromium.org', uriBookmark.uri);

    chrome.test.succeed();
  }
];

var scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCallback(function() {
  chrome.test.runTests(tests);
});
