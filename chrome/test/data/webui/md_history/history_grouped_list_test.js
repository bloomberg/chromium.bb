// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_grouped_list_test', function() {
  function registerTests() {
    suite('history-grouped-list', function() {
      var app;
      var toolbar;
      var groupedList;
      var SIMPLE_RESULTS;
      var PER_DAY_RESULTS;
      var PER_MONTH_RESULTS;
      suiteSetup(function(done) {
        app = $('history-app');
        app.grouped_ = true;

        toolbar = app.$['toolbar'];

        SIMPLE_RESULTS = [
          createHistoryEntry('2016-03-16', 'https://www.google.com/'),
          createHistoryEntry('2016-03-16', 'https://en.wikipedia.org/DankMeme'),
          createHistoryEntry('2016-03-16 10:00', 'https://www.example.com'),
          createHistoryEntry('2016-03-16',
                             'https://www.google.com/?q=yoloswaggins'),
        ];

        PER_DAY_RESULTS = [
          createHistoryEntry('2016-03-13', 'https://en.wikipedia.org'),
          createHistoryEntry('2016-03-13', 'https://www.youtube.com'),
          createHistoryEntry('2016-03-13', 'https://en.wikipedia.org'),
          createHistoryEntry('2016-03-11', 'https://en.wikipedia.org'),
          createHistoryEntry('2016-03-10', 'https://en.wikipedia.org')
        ];

        PER_MONTH_RESULTS = [
          createHistoryEntry('2016-03-13', 'https://en.wikipedia.org'),
          createHistoryEntry('2016-03-13', 'https://www.youtube.com'),
          createHistoryEntry('2016-03-13', 'https://en.wikipedia.org'),
          createHistoryEntry('2016-03-1', 'https://en.wikipedia.org'),
          createHistoryEntry('2016-03-1', 'https://en.wikipedia.org')
        ];
        flush(function() {
          groupedList = app.$$('#history-grouped-list');
          done();
        });
      });

      test('grouped ui is shown', function(done) {
        assertTrue(!!toolbar.$$('#grouped-buttons-container'));

        // History list is shown at first.
        assertEquals('history-list', app.$['content'].selected);

        // Switching to week or month causes grouped history list to be shown.
        app.set('queryState_.range', HistoryRange.WEEK);
        assertEquals('history-grouped-list', app.$['content'].selected);

        app.set('queryState_.range', HistoryRange.ALL_TIME);
        assertEquals('history-list', app.$['content'].selected);

        app.set('queryState_.range', HistoryRange.MONTH);
        assertEquals('history-grouped-list', app.$['content'].selected);
        done();
      });

      test('items grouped by domain', function(done) {
        app.set('queryState_.range', HistoryRange.WEEK);
        app.historyResult(createHistoryInfo(), SIMPLE_RESULTS);
        flush(function() {
          var data = groupedList.groupedHistoryData_;
          // 1 card for the day with 3 domains.
          assertEquals(1, data.length);
          assertEquals(3, data[0].domains.length);

          // Most visits at the top.
          assertEquals(2, data[0].domains[0].visits.length);
          assertEquals(1, data[0].domains[1].visits.length);
          assertEquals(1, data[0].domains[2].visits.length);
          done();
        });
      });

      test('items grouped by day in week view', function(done) {
        app.set('queryState_.range', HistoryRange.WEEK);
        app.historyResult(createHistoryInfo(), PER_DAY_RESULTS);
        flush(function() {
          var data = groupedList.groupedHistoryData_;

          // 3 cards.
          assertEquals(3, data.length);

          assertEquals(2, data[0].domains.length);
          assertEquals(2, data[0].domains[0].visits.length);
          assertEquals(1, data[0].domains[1].visits.length);

          assertEquals(1, data[1].domains.length);
          assertEquals(1, data[1].domains[0].visits.length);

          assertEquals(1, data[2].domains.length);
          assertEquals(1, data[2].domains[0].visits.length);
          done();
        });
      });

      test('items grouped by month in month view', function(done) {
        app.set('queryState_.range', HistoryRange.MONTH);
        app.historyResult(createHistoryInfo(), PER_MONTH_RESULTS);
        flush(function() {
          var data = groupedList.groupedHistoryData_;

          // 1 card.
          assertEquals(1, data.length);

          assertEquals(2, data[0].domains.length);
          assertEquals(4, data[0].domains[0].visits.length);
          assertEquals(1, data[0].domains[1].visits.length);

          done();
        });
      });

      test('items rendered when expanded', function(done) {
        app.set('queryState_.range', HistoryRange.WEEK);
        app.historyResult(createHistoryInfo(), SIMPLE_RESULTS);
        flush(function() {
          var getItems = function() {
            return Polymer.dom(groupedList.root)
                .querySelectorAll('history-item');
          };

          assertEquals(0, getItems().length);
          MockInteractions.tap(groupedList.$$('.domain-heading'));
          flush(function() {
            assertEquals(2, getItems().length);
            done();
          });
        });
      });

      teardown(function() {
        app.set('queryState_.results', []);
        app.set('queryState_.searchedTerm', '');
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
