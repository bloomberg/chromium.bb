// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_item_test', function() {
  function registerTests() {
    suite('history-item', function() {
      var element;
      var TEST_HISTORY_RESULTS;
      var SEARCH_HISTORY_RESULTS;

      suiteSetup(function() {
        element = $('history-app').$['history'].$['infinite-list'];
        TEST_HISTORY_RESULTS = [
          createHistoryEntry('2016-03-16 10:00', 'http://www.google.com'),
          createHistoryEntry('2016-03-16 9:00', 'http://www.example.com'),
          createHistoryEntry('2016-03-16 7:01', 'http://www.badssl.com'),
          createHistoryEntry('2016-03-16 7:00', 'http://www.website.com'),
          createHistoryEntry('2016-03-16 4:00', 'http://www.website.com'),
          createHistoryEntry('2016-03-15 11:00', 'http://www.example.com'),
        ];

        SEARCH_HISTORY_RESULTS = [
          createSearchEntry('2016-03-16', "http://www.google.com"),
          createSearchEntry('2016-03-14 11:00', "http://calendar.google.com"),
          createSearchEntry('2016-03-14 10:00', "http://mail.google.com")
        ];
      });

      test('basic separator insertion', function() {
        element.addNewResults(TEST_HISTORY_RESULTS);
        return flush().then(function() {
          // Check that the correct number of time gaps are inserted.
          var items =
              Polymer.dom(element.root).querySelectorAll('history-item');

          assertTrue(items[0].hasTimeGap);
          assertTrue(items[1].hasTimeGap);
          assertFalse(items[2].hasTimeGap);
          assertTrue(items[3].hasTimeGap);
          assertFalse(items[4].hasTimeGap);
          assertFalse(items[5].hasTimeGap);
        });
      });

      test('separator insertion for search', function() {
        element.addNewResults(SEARCH_HISTORY_RESULTS);
        element.searchedTerm = 'search';

        return flush().then(function() {
          var items =
              Polymer.dom(element.root).querySelectorAll('history-item');

          assertTrue(items[0].hasTimeGap);
          assertFalse(items[1].hasTimeGap);
          assertFalse(items[2].hasTimeGap);
        });
      });

      test('separator insertion after deletion', function() {
        element.addNewResults(TEST_HISTORY_RESULTS);
        return flush().then(function() {
          var items =
              Polymer.dom(element.root).querySelectorAll('history-item');

          element.removeItemsByPath(['historyData_.3']);
          assertEquals(5, element.historyData_.length);

          // Checks that a new time gap separator has been inserted.
          assertTrue(items[2].hasTimeGap);

          element.removeItemsByPath(['historyData_.3']);

          // Checks time gap separator is removed.
          assertFalse(items[2].hasTimeGap);
        });
      });

      test('remove bookmarks', function() {
        element.addNewResults(TEST_HISTORY_RESULTS);
        return flush().then(function() {
          element.set('historyData_.1.starred', true);
          element.set('historyData_.5.starred', true);
          return flush();
        }).then(function() {

          items = Polymer.dom(element.root).querySelectorAll('history-item');

          items[1].$$('#bookmark-star').focus();
          MockInteractions.tap(items[1].$$('#bookmark-star'));

          // Check that focus is shifted to overflow menu icon.
          assertEquals(items[1].root.activeElement, items[1].$['menu-button']);
          // Check that all items matching this url are unstarred.
          assertEquals(element.historyData_[1].starred, false);
          assertEquals(element.historyData_[5].starred, false);
        });
      });

      teardown(function() {
        element.historyData_ = [];
        element.searchedTerm = '';
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
