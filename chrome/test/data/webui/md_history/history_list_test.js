// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_list_test', function() {
  function registerTests() {
    suite('history-list', function() {
      var element;
      var toolbar;
      var TEST_HISTORY_RESULTS;
      var ADDITIONAL_RESULTS;

      suiteSetup(function() {
        element = $('history-list');
        toolbar = $('toolbar');

        TEST_HISTORY_RESULTS = [
          createHistoryEntry('2016-03-15', 'https://www.google.com'),
          createHistoryEntry('2016-03-14', 'https://en.wikipedia.org'),
          createHistoryEntry('2016-03-13 10:00', 'https://www.example.com'),
          createHistoryEntry('2016-03-13 9:00', 'https://www.google.com')
        ];

        ADDITIONAL_RESULTS = [
          createHistoryEntry('2016-03-12 10:00', 'https://en.wikipedia.org'),
          createHistoryEntry('2016-03-12 9:50', 'https://www.youtube.com'),
          createHistoryEntry('2016-03-11', 'https://www.google.com'),
          createHistoryEntry('2016-03-10', 'https://www.example.com')
        ];
      });

      test('setting first and last items', function() {
        element.addNewResults(TEST_HISTORY_RESULTS, '');
        assertTrue(element.historyData[0].isFirstItem);
        assertTrue(element.historyData[0].isLastItem);
        assertTrue(element.historyData[2].isFirstItem);
        assertFalse(element.historyData[2].isLastItem);
        assertFalse(element.historyData[3].isFirstItem);
        assertTrue(element.historyData[3].isLastItem);
      });

      test('cancelling selection of multiple items', function(done) {
        element.addNewResults(TEST_HISTORY_RESULTS, '');
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
        element.addNewResults(TEST_HISTORY_RESULTS, '');
        element.addNewResults(ADDITIONAL_RESULTS, '');

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
        element.addNewResults(TEST_HISTORY_RESULTS, '');
        // Ensure that the correct identifying data is being used for removal.
        registerMessageCallback('removeVisits', this, function (toBeRemoved) {

          assertEquals(toBeRemoved[0].url, element.historyData[0].url);
          assertEquals(toBeRemoved[1].url, element.historyData[1].url);
          assertEquals(toBeRemoved[0].timestamps,
                       element.historyData[0].allTimestamps);
          assertEquals(toBeRemoved[1].timestamps,
                       element.historyData[1].allTimestamps);

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
        element.addNewResults(TEST_HISTORY_RESULTS, '');
        element.addNewResults(ADDITIONAL_RESULTS, '');
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
                         '2016-03-15');
            assertEquals(element.historyData[2].dateRelativeDay,
                         '2016-03-13');
            assertEquals(element.historyData[4].dateRelativeDay,
                         '2016-03-11');

            // Checks that the first and last items have been reset correctly.
            assertTrue(element.historyData[2].isFirstItem);
            assertTrue(element.historyData[3].isLastItem);
            assertTrue(element.historyData[4].isFirstItem);
            assertTrue(element.historyData[4].isLastItem)

            done();
          });
        });
      });

      test('search results display with correct item title', function(done) {
        element.addNewResults(
            [createHistoryEntry('2016-03-15', 'https://www.google.com')],
            'Google');

        flush(function() {
          assertTrue(element.historyData[0].isFirstItem);
          var heading =
              element.$$('history-item').$$('#date-accessed').textContent;
          var title = element.$$('history-item').$.title;

          // Check that the card title displays the search term somewhere.
          var index = heading.indexOf('Google');
          assertTrue(index != -1);

          // Check that the search term is bolded correctly in the history-item.
          assertGT(title.innerHTML.indexOf('<b>google</b>'), -1);
          done();
        });
      });

      test('correct display message when no history available', function(done) {
        element.addNewResults([], '');

        flush(function() {
          assertFalse(element.$['no-results'].hidden);
          assertTrue(element.$['infinite-list'].hidden);

          element.addNewResults(TEST_HISTORY_RESULTS, '');

          flush(function() {
            assertTrue(element.$['no-results'].hidden);
            assertFalse(element.$['infinite-list'].hidden);
            done();
          });
        });
      });

      teardown(function() {
        element.historyData = [];
        registerMessageCallback('removeVisits', this, undefined);
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
