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
        element = $('history-app').$['history-list'];
        toolbar = $('history-app').$['toolbar'];

        TEST_HISTORY_RESULTS = [
          createHistoryEntry('2016-03-15', 'https://www.google.com'),
          createHistoryEntry('2016-03-14 10:00', 'https://www.example.com'),
          createHistoryEntry('2016-03-14 9:00', 'https://www.google.com'),
          createHistoryEntry('2016-03-13', 'https://en.wikipedia.org')
        ];

        ADDITIONAL_RESULTS = [
          createHistoryEntry('2016-03-13 10:00', 'https://en.wikipedia.org'),
          createHistoryEntry('2016-03-13 9:50', 'https://www.youtube.com'),
          createHistoryEntry('2016-03-11', 'https://www.google.com'),
          createHistoryEntry('2016-03-10', 'https://www.example.com')
        ];
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

      test('setting first and last items', function(done) {
        element.addNewResults(TEST_HISTORY_RESULTS, '');

        flush(function() {
          var items =
              Polymer.dom(element.root).querySelectorAll('history-item');
          assertTrue(items[0].isCardStart);
          assertTrue(items[0].isCardEnd);
          assertFalse(items[1].isCardEnd);
          assertFalse(items[2].isCardStart);
          assertTrue(items[2].isCardEnd);
          assertTrue(items[3].isCardStart);
          assertTrue(items[3].isCardEnd);

          done();
        });
      });

      test('updating history results', function(done) {
        element.addNewResults(TEST_HISTORY_RESULTS, '');
        element.addNewResults(ADDITIONAL_RESULTS, '');

        flush(function() {
          var items =
              Polymer.dom(element.root).querySelectorAll('history-item');
          assertTrue(items[3].isCardStart);
          assertTrue(items[5].isCardEnd);

          assertTrue(items[6].isCardStart);
          assertTrue(items[6].isCardEnd);

          assertTrue(items[7].isCardStart);
          assertTrue(items[7].isCardEnd);

          done();
        });
      });

      test('deleting multiple items from view', function(done) {
        element.addNewResults(TEST_HISTORY_RESULTS, '');
        element.addNewResults(ADDITIONAL_RESULTS, '');
        flush(function() {
          items = Polymer.dom(element.root).querySelectorAll('history-item');

          element.removeDeletedHistory_([
            element.historyData[2], element.historyData[5],
            element.historyData[7]
          ]);

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
            assertTrue(items[2].isCardStart);
            assertTrue(items[3].isCardEnd);
            assertTrue(items[4].isCardStart);
            assertTrue(items[4].isCardEnd)

            done();
          });
        });
      });

      test('search results display with correct item title', function(done) {
        element.addNewResults(
            [createHistoryEntry('2016-03-15', 'https://www.google.com')],
            'Google');

        flush(function() {
          var item = element.$$('history-item');
          assertTrue(item.isCardStart);
          var heading = item.$$('#date-accessed').textContent;
          var title = item.$.title;

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
