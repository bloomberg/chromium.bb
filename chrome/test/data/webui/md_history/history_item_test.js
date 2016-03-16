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
        element = $('history-list');
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
          createSearchEntry('2016-03-14', "http://calendar.google.com"),
          createSearchEntry('2016-03-14', "http://mail.google.com")
        ];
      });

      setup(function() {
        element.addNewResults(TEST_HISTORY_RESULTS, '');
      });

      test('basic separator insertion', function(done) {
        flush(function() {
          // Check that the correct number of time gaps are inserted.
          var items =
              Polymer.dom(element.root).querySelectorAll('history-item');

          assertTrue(items[0].item.needsTimeGap);
          assertTrue(items[1].item.needsTimeGap);
          assertFalse(items[2].item.needsTimeGap);
          assertTrue(items[3].item.needsTimeGap);
          assertFalse(items[4].item.needsTimeGap);
          assertFalse(items[5].item.needsTimeGap);

          done();
        });
      });

      test('separator insertion for search', function(done) {
        element.addNewResults(SEARCH_HISTORY_RESULTS, 'search');
        flush(function() {
          var items =
              Polymer.dom(element.root).querySelectorAll('history-item');

          assertTrue(items[0].item.needsTimeGap);
          assertFalse(items[1].item.needsTimeGap);
          assertFalse(items[2].item.needsTimeGap);

          done();
        });
      });

      test('separator insertion after deletion', function(done) {
        flush(function() {
          var items =
              Polymer.dom(element.root).querySelectorAll('history-item');

          element.set('historyData.3.selected', true);
          items[3].onCheckboxSelected_();

          element.removeDeletedHistory(1);
          assertEquals(element.historyData.length, 5);

          // Checks that a new time gap separator has been inserted.
          assertTrue(items[2].item.needsTimeGap);

          element.set('historyData.3.selected', true);
          items[3].onCheckboxSelected_();
          element.removeDeletedHistory(1);

          // Checks time gap separator is removed.
          assertFalse(items[2].item.needsTimeGap);
          done();
        });
      });

      teardown(function() {
        element.historyData = [];
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
