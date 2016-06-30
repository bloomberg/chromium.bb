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
        element = $('history-app').$['history-list'];
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

          element.removeDeletedHistory_([element.historyData_[3]]);
          assertEquals(5, element.historyData_.length);

          // Checks that a new time gap separator has been inserted.
          assertTrue(items[2].hasTimeGap);

          element.removeDeletedHistory_([element.historyData_[3]]);

          // Checks time gap separator is removed.
          assertFalse(items[2].hasTimeGap);
        });
      });

      test('long titles are trimmed', function() {
        var item = document.createElement('history-item');
        var longtitle = '0123456789'.repeat(100);
        item.item =
            createHistoryEntry('2016-06-30', 'http://example.com/' + longtitle);

        var label = item.$$('history-searched-label');
        assertEquals(TITLE_MAX_LENGTH, label.title.length);
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
