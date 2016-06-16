// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_list_test', function() {
  function registerTests() {
    suite('history-list', function() {
      var app;
      var element;
      var toolbar;
      var TEST_HISTORY_RESULTS;
      var ADDITIONAL_RESULTS;

      suiteSetup(function() {
        app = $('history-app');
        element = app.$['history-list'];
        toolbar = app.$['toolbar'];

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

      test('cancelling selection of multiple items', function() {
        app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
        return flush().then(function() {
          var items = Polymer.dom(element.root)
              .querySelectorAll('history-item');

          MockInteractions.tap(items[2].$.checkbox);
          MockInteractions.tap(items[3].$.checkbox);

          // Make sure that the array of data that determines whether or not an
          // item is selected is what we expect after selecting the two items.
          assertFalse(element.historyData_[0].selected);
          assertFalse(element.historyData_[1].selected);
          assertTrue(element.historyData_[2].selected);
          assertTrue(element.historyData_[3].selected);

          toolbar.onClearSelectionTap_();

          // Make sure that clearing the selection updates both the array and
          // the actual history-items affected.
          assertFalse(element.historyData_[0].selected);
          assertFalse(element.historyData_[1].selected);
          assertFalse(element.historyData_[2].selected);
          assertFalse(element.historyData_[3].selected);

          assertFalse(items[2].$.checkbox.checked);
          assertFalse(items[3].$.checkbox.checked);
        });
      });

      test('setting first and last items', function() {
        app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);

        return flush().then(function() {
          var items =
              Polymer.dom(element.root).querySelectorAll('history-item');
          assertTrue(items[0].isCardStart);
          assertTrue(items[0].isCardEnd);
          assertFalse(items[1].isCardEnd);
          assertFalse(items[2].isCardStart);
          assertTrue(items[2].isCardEnd);
          assertTrue(items[3].isCardStart);
          assertTrue(items[3].isCardEnd);
        });
      });

      test('updating history results', function() {
        app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
        app.historyResult(createHistoryInfo(), ADDITIONAL_RESULTS);

        return flush().then(function() {
          var items =
              Polymer.dom(element.root).querySelectorAll('history-item');
          assertTrue(items[3].isCardStart);
          assertTrue(items[5].isCardEnd);

          assertTrue(items[6].isCardStart);
          assertTrue(items[6].isCardEnd);

          assertTrue(items[7].isCardStart);
          assertTrue(items[7].isCardEnd);
        });
      });

      test('deleting multiple items from view', function() {
        app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
        app.historyResult(createHistoryInfo(), ADDITIONAL_RESULTS);
        return flush().then(function() {
          items = Polymer.dom(element.root).querySelectorAll('history-item');

          element.removeDeletedHistory_([
            element.historyData_[2], element.historyData_[5],
            element.historyData_[7]
          ]);

          return flush();
        }).then(function() {
          items = Polymer.dom(element.root).querySelectorAll('history-item');

          assertEquals(element.historyData_.length, 5);
          assertEquals(element.historyData_[0].dateRelativeDay,
                       '2016-03-15');
          assertEquals(element.historyData_[2].dateRelativeDay,
                       '2016-03-13');
          assertEquals(element.historyData_[4].dateRelativeDay,
                       '2016-03-11');

          // Checks that the first and last items have been reset correctly.
          assertTrue(items[2].isCardStart);
          assertTrue(items[3].isCardEnd);
          assertTrue(items[4].isCardStart);
          assertTrue(items[4].isCardEnd)
        });
      });

      test('search results display with correct item title', function() {
        app.historyResult(createHistoryInfo(),
            [createHistoryEntry('2016-03-15', 'https://www.google.com')]);
        element.searchedTerm = 'Google';

        return flush().then(function() {
          var item = element.$$('history-item');
          assertTrue(item.isCardStart);
          var heading = item.$$('#date-accessed').textContent;
          var title = item.$.title;

          // Check that the card title displays the search term somewhere.
          var index = heading.indexOf('Google');
          assertTrue(index != -1);

          // Check that the search term is bolded correctly in the history-item.
          assertGT(
              title.children[0].$.container.innerHTML.indexOf('<b>google</b>'),
              -1);
        });
      });

      test('correct display message when no history available', function() {
        app.historyResult(createHistoryInfo(), []);

        return flush().then(function() {
          assertFalse(element.$['no-results'].hidden);
          assertTrue(element.$['infinite-list'].hidden);

          app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
          return flush();
        }).then(function() {
          assertTrue(element.$['no-results'].hidden);
          assertFalse(element.$['infinite-list'].hidden);
        });
      });

      test('more from this site sends and sets correct data', function(done) {
        app.queryingDisabled_ = false;
        registerMessageCallback('queryHistory', this, function (info) {
          assertEquals('example.com', info[0]);
          flush().then(function() {
            assertEquals(
                'example.com',
                toolbar.$['main-toolbar'].getSearchField().getValue());
            done();
          });
        });

        element.$.sharedMenu.itemData = {domain: 'example.com'};
        MockInteractions.tap(element.$.menuMoreButton);
      });

      test('changing search deselects items', function() {
        app.historyResult(
            createHistoryInfo('ex'),
            [createHistoryEntry('2016-06-9', 'https://www.example.com')]);
        return flush().then(function() {
          var item = element.$$('history-item');
          MockInteractions.tap(item.$.checkbox);

          assertEquals(1, toolbar.count);

          app.historyResult(
              createHistoryInfo('ample'),
              [createHistoryEntry('2016-06-9', 'https://www.example.com')]);
          assertEquals(0, toolbar.count);
        });
      });

      teardown(function() {
        element.historyData_ = [];
        element.searchedTerm = '';
        registerMessageCallback('removeVisits', this, undefined);
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
