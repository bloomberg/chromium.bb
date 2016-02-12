// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_list_test', function() {
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
    suite('history-list', function() {
      var element;
      var toolbar;

      suiteSetup(function() {
        element = $('history-list');
        toolbar = $('toolbar');
      });

      setup(function() {
        element.addNewResults(TEST_HISTORY_RESULTS);
      });

      test('setting first and last items', function() {
        assertTrue(element.historyData[0].isLastItem);
        assertTrue(element.historyData[0].isFirstItem);
        assertTrue(element.historyData[2].isFirstItem);
        assertTrue(element.historyData[3].isLastItem);
      });

      test('cancelling selection of multiple items', function(done) {
        flush(function() {
          var items = Polymer.dom(element.root)
              .querySelectorAll('history-item');

          MockInteractions.tap(items[2].$.checkbox);
          MockInteractions.tap(items[3].$.checkbox);

          // Make sure that the array of data that determines whether or not an
          // item is selected is what we expect after selecting the two items.
          assertFalse(element.historyData[0].selected);
          assertFalse(element.historyData[1].selected);
          assertTrue(element.historyData[2].selected);
          assertTrue(element.historyData[3].selected);

          toolbar.onClearSelectionTap_();

          // Make sure that clearing the selection updates both the array and
          // the actual history-items affected.
          assertFalse(element.historyData[0].selected);
          assertFalse(element.historyData[1].selected);
          assertFalse(element.historyData[2].selected);
          assertFalse(element.historyData[3].selected);

          assertFalse(items[2].$.checkbox.checked);
          assertFalse(items[3].$.checkbox.checked);

          done();
        });
      });

      test('updating history results', function(done) {
        element.addNewResults(ADDITIONAL_RESULTS);

        flush(function() {
          assertTrue(element.historyData[2].isFirstItem);
          assertTrue(element.historyData[5].isLastItem);

          assertTrue(element.historyData[6].isFirstItem);
          assertTrue(element.historyData[6].isLastItem);

          assertTrue(element.historyData[7].isFirstItem);
          assertTrue(element.historyData[7].isLastItem);

          done();
        });
      });

      test('removeVisits for multiple items', function(done) {
        // Ensure that the correct identifying data is being used for removal.
        registerMessageCallback('removeVisits', this, function (toBeRemoved) {

          assertEquals(toBeRemoved[0].url, element.historyData[0].url);
          assertEquals(toBeRemoved[1].url, element.historyData[1].url);
          assertEquals(toBeRemoved[0].timestamps,
                       element.historyData[0].timestamps);
          assertEquals(toBeRemoved[1].timestamps,
                       element.historyData[1].timestamps);

          done();
        });

        flush(function() {
          items = Polymer.dom(element.root).querySelectorAll('history-item');

          MockInteractions.tap(items[0].$.checkbox);
          MockInteractions.tap(items[1].$.checkbox);

          toolbar.onDeleteTap_();
        });
      });

      test('deleting multiple items from view', function(done) {
        element.addNewResults(ADDITIONAL_RESULTS);
        flush(function() {
          items = Polymer.dom(element.root).querySelectorAll('history-item');

          // Selects the checkboxes.
          element.set('historyData.2.selected', true);
          items[2].onCheckboxSelected_();
          element.set('historyData.5.selected', true);
          items[5].onCheckboxSelected_();
          element.set('historyData.7.selected', true);
          items[7].onCheckboxSelected_();

          element.removeDeletedHistory(3);

          flush(function() {
            items = Polymer.dom(element.root).querySelectorAll('history-item');

            assertEquals(element.historyData.length, 5);
            assertEquals(element.historyData[0].dateRelativeDay,
                         "Today - Wednesday, December 9, 2015");
            assertEquals(element.historyData[2].dateRelativeDay,
                         "Monday, December 7, 2015");
            assertEquals(element.historyData[4].dateRelativeDay,
                         "Sunday, December 6, 2015");

            // Checks that the first and last items have been reset correctly.
            assertTrue(element.historyData[2].isFirstItem);
            assertTrue(element.historyData[3].isLastItem);
            assertTrue(element.historyData[4].isFirstItem);
            assertTrue(element.historyData[4].isLastItem)

            done();
          });
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
