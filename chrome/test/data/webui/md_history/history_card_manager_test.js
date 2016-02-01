// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_card_manager_test', function() {
  // Array of test history data.
  var TEST_HISTORY_RESULTS = [
    {
      "dateRelativeDay": "Today - Wednesday, December 9, 2015",
      "url": "https://www.google.com"
    },
    {
      "dateRelativeDay": "Yesterday - Tuesday, December 8, 2015",
      "url": "https://en.wikipedia.com"
    },
    {
      "dateRelativeDay": "Monday, December 7, 2015",
      "url": "https://www.example.com"
    },
    {
      "dateRelativeDay": "Monday, December 7, 2015",
      "url": "https://www.google.com"
    }
  ];

  var ADDITIONAL_RESULTS = [
    {
      "dateRelativeDay": "Monday, December 7, 2015",
      "url": "https://en.wikipedia.com"
    },
    {
      "dateRelativeDay": "Monday, December 7, 2015",
      "url": "https://www.youtube.com"
    },
    {
      "dateRelativeDay": "Sunday, December 6, 2015",
      "url": "https://www.google.com"
    },
    {
      "dateRelativeDay": "Saturday, December 5, 2015",
      "url": "https://www.example.com"
    }
  ];

  function registerTests() {
    suite('history-card-manager', function() {
      var element;

      suiteSetup(function() {
        element = $('history-card-manager');
      });

      setup(function() {
        element.addNewResults(TEST_HISTORY_RESULTS);
      });

      test('splitting items by day', function() {
        assertEquals(3, element.historyDataByDay_.length);

        // Ensure that the output is in reversed date order.
        assertEquals("Today - Wednesday, December 9, 2015",
                     element.historyDataByDay_[0].date);
        assertEquals("Yesterday - Tuesday, December 8, 2015",
                     element.historyDataByDay_[1].date);
        assertEquals("Monday, December 7, 2015",
                     element.historyDataByDay_[2].date);
        assertEquals("https://www.example.com",
                     element.historyDataByDay_[2].historyItems[0].url);
        assertEquals("https://www.google.com",
                     element.historyDataByDay_[2].historyItems[1].url);
      });

      test('cancelling selection of multiple items', function(done) {
        var toolbar = $('toolbar');

        flush(function() {
          var cards = Polymer.dom(element.root)
              .querySelectorAll('history-card');
          var items = Polymer.dom(cards[2].root)
              .querySelectorAll('history-item');

          MockInteractions.tap(items[0].$.checkbox);
          MockInteractions.tap(items[1].$.checkbox);

          // Make sure that the array of data that determines whether or not an
          // item is selected is what we expect after selecting the two items.
          assertFalse(element.historyDataByDay_[0].historyItems[0].selected);
          assertFalse(element.historyDataByDay_[1].historyItems[0].selected);
          assertTrue(element.historyDataByDay_[2].historyItems[0].selected);
          assertTrue(element.historyDataByDay_[2].historyItems[1].selected);

          toolbar.onClearSelectionTap_();

          // Make sure that clearing the selection updates both the array and
          // the actual history-items affected.
          assertFalse(element.historyDataByDay_[0].historyItems[0].selected);
          assertFalse(element.historyDataByDay_[1].historyItems[0].selected);
          assertFalse(element.historyDataByDay_[2].historyItems[0].selected);
          assertFalse(element.historyDataByDay_[2].historyItems[1].selected);

          assertFalse(items[0].$.checkbox.checked);
          assertFalse(items[1].$.checkbox.checked);

          done();
        });
      });

      test('updating history results', function() {
        element.addNewResults(ADDITIONAL_RESULTS);

        assertEquals(5, element.historyDataByDay_.length);

        // Ensure that the output is still in reverse order.
        assertEquals("Monday, December 7, 2015",
                     element.historyDataByDay_[2].date);
        assertEquals("Sunday, December 6, 2015",
                     element.historyDataByDay_[3].date);
        assertEquals("Saturday, December 5, 2015",
                     element.historyDataByDay_[4].date);

        // Ensure that the correct items have been appended to the list.
        assertEquals("https://en.wikipedia.com",
                     element.historyDataByDay_[2].historyItems[2].url);
        assertEquals("https://www.youtube.com",
                     element.historyDataByDay_[2].historyItems[3].url);
        assertEquals("https://www.google.com",
                     element.historyDataByDay_[3].historyItems[0].url);
        assertEquals("https://www.example.com",
                     element.historyDataByDay_[4].historyItems[0].url);
      });

      teardown(function() {
        element.historyDataByDay_ = [];
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
