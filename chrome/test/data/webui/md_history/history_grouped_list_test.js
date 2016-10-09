// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_grouped_list_test', function() {
  function registerTests() {
    suite('history-grouped-list', function() {
      var app;
      var toolbar;
      var groupedList;
      var sidebar;
      var listContainer;

      var SIMPLE_RESULTS;
      var PER_DAY_RESULTS;
      var PER_MONTH_RESULTS;

      suiteSetup(function() {
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
      });

      setup(function() {
        app = replaceApp();
        app.grouped_ = true;

        listContainer = app.$['history'];
        toolbar = app.$['toolbar'];
        sidebar = app.$['content-side-bar'];
        return PolymerTest.flushTasks().then(function() {
          groupedList = app.$.history.$$('#grouped-list');
          assertTrue(!!groupedList);
        });
      });

      test('grouped ui is shown', function() {
        assertEquals('history', sidebar.$.menu.selected);
        assertTrue(!!toolbar.$$('#grouped-buttons-container'));

        var content = app.$['history'].$['content'];

        // History list is shown at first.
        assertEquals('infinite-list', content.selected);

        // Switching to week or month causes grouped history list to be shown.
        app.set('queryState_.range', HistoryRange.WEEK);
        assertEquals('grouped-list', content.selected);
        assertEquals('history', sidebar.$.menu.selected);

        app.set('queryState_.range', HistoryRange.ALL_TIME);
        assertEquals('infinite-list', content.selected);
        assertEquals('history', sidebar.$.menu.selected);

        app.set('queryState_.range', HistoryRange.MONTH);
        assertEquals('grouped-list', content.selected);
        assertEquals('history', sidebar.$.menu.selected);
      });

      test('items grouped by domain', function() {
        app.set('queryState_.range', HistoryRange.WEEK);
        var info = createHistoryInfo();
        info.queryStartTime = 'Yesterday';
        info.queryEndTime = 'Now';
        app.historyResult(info, SIMPLE_RESULTS);
        return PolymerTest.flushTasks().then(function() {
          var data = groupedList.groupedHistoryData_;
          // 1 card for the day with 3 domains.
          assertEquals(1, data.length);
          assertEquals(3, data[0].domains.length);

          // Most visits at the top.
          assertEquals(2, data[0].domains[0].visits.length);
          assertEquals(1, data[0].domains[1].visits.length);
          assertEquals(1, data[0].domains[2].visits.length);

          // Ensure the toolbar displays the correct begin and end time.
          assertEquals('Yesterday', toolbar.queryStartTime);
          assertEquals('Now', toolbar.queryEndTime);
        });
      });

      test('items grouped by day in week view', function() {
        app.set('queryState_.range', HistoryRange.WEEK);
        app.historyResult(createHistoryInfo(), PER_DAY_RESULTS);
        return PolymerTest.flushTasks().then(function() {
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
        });
      });

      test('items grouped by month in month view', function() {
        app.set('queryState_.range', HistoryRange.MONTH);
        app.historyResult(createHistoryInfo(), PER_MONTH_RESULTS);
        return PolymerTest.flushTasks().then(function() {
          var data = groupedList.groupedHistoryData_;

          // 1 card.
          assertEquals(1, data.length);

          assertEquals(2, data[0].domains.length);
          assertEquals(4, data[0].domains[0].visits.length);
          assertEquals(1, data[0].domains[1].visits.length);
        });
      });

      test('items rendered when expanded', function() {
        app.set('queryState_.range', HistoryRange.WEEK);
        app.historyResult(createHistoryInfo(), SIMPLE_RESULTS);

        return PolymerTest.flushTasks().then(function() {
          assertEquals(
              0, polymerSelectAll(groupedList, 'history-item').length);
          MockInteractions.tap(groupedList.$$('.domain-heading'));
          return PolymerTest.flushTasks();
        }).then(function() {
          assertEquals(
              2, polymerSelectAll(groupedList, 'history-item').length);
        });
      });

      test('shift selection in week view', function() {
        var results = [
          createHistoryEntry('2016-03-13', 'https://en.wikipedia.org/a'),
          createHistoryEntry('2016-03-13', 'https://en.wikipedia.org/b'),
          createHistoryEntry('2016-03-13', 'https://en.wikipedia.org/c'),
          createHistoryEntry('2016-03-13', 'https://en.wikipedia.org/d'),
          createHistoryEntry('2016-03-13', 'https://www.youtube.com/a'),
          createHistoryEntry('2016-03-13', 'https://www.youtube.com/b'),
          createHistoryEntry('2016-03-11', 'https://en.wikipedia.org'),
          createHistoryEntry('2016-03-10', 'https://www.youtube.com')
        ];
        app.set('queryState_.range', HistoryRange.WEEK);
        app.historyResult(createHistoryInfo(), results);

        return waitForEvent(groupedList, 'dom-change', function() {
          return polymerSelectAll(
              groupedList, '.dropdown-indicator').length == 4;
        }).then(function() {
          polymerSelectAll(groupedList, '.dropdown-indicator').
              forEach(MockInteractions.tap);

          return PolymerTest.flushTasks();
        }).then(function() {
          var items = polymerSelectAll(groupedList, 'history-item');

          MockInteractions.tap(items[0].$.checkbox);
          assertDeepEquals(
              [true, false, false, false],
              groupedList.groupedHistoryData_[0].domains[0].visits.map(
                  i => i.selected));

          // Shift-select to the third item.
          shiftClick(items[2].$.checkbox);
          assertDeepEquals(
              [true, true, true, false],
              groupedList.groupedHistoryData_[0].domains[0].visits.map(
                  i => i.selected));

          // Shift-selecting to another domain selects as per usual.
          shiftClick(items[5].$.checkbox);
          assertDeepEquals(
              [true, true, true, false],
              groupedList.groupedHistoryData_[0].domains[0].visits.map(
                  i => i.selected));
          assertDeepEquals(
              [false, true],
              groupedList.groupedHistoryData_[0].domains[1].visits.map(
                  i => i.selected));
        });
      });

      test('items deletion in week view', function(done) {
        var results = [
          createHistoryEntry('2016-03-13', 'https://en.wikipedia.org/a'),
          createHistoryEntry('2016-03-13', 'https://en.wikipedia.org/b'),
          createHistoryEntry('2016-03-13', 'https://en.wikipedia.org/c'),
          createHistoryEntry('2016-03-13', 'https://en.wikipedia.org/d'),
          createHistoryEntry('2016-03-13', 'https://www.youtube.com/a'),
          createHistoryEntry('2016-03-13', 'https://www.youtube.com/b'),
          createHistoryEntry('2016-03-11', 'https://en.wikipedia.org'),
          createHistoryEntry('2016-03-10', 'https://www.youtube.com')
        ];
        app.set('queryState_.range', HistoryRange.WEEK);
        app.historyResult(createHistoryInfo(), results);

        waitForEvent(groupedList, 'dom-change', function() {
          return polymerSelectAll(
              groupedList, '.dropdown-indicator').length == 4;
        }).then(function() {
          polymerSelectAll(groupedList, '.dropdown-indicator').
              forEach(MockInteractions.tap);

          return PolymerTest.flushTasks();
        }).then(function() {
          var items = polymerSelectAll(groupedList, 'history-item');

          MockInteractions.tap(items[0].$.checkbox);
          MockInteractions.tap(items[2].$.checkbox);
          MockInteractions.tap(items[4].$.checkbox);
          MockInteractions.tap(items[5].$.checkbox);
          MockInteractions.tap(items[6].$.checkbox);
          MockInteractions.tap(items[7].$.checkbox);

          // Select and deselect item 1.
          MockInteractions.tap(items[1].$.checkbox);
          MockInteractions.tap(items[1].$.checkbox);

          return PolymerTest.flushTasks();
        }).then(function() {
          MockInteractions.tap(app.$.toolbar.$$('#delete-button'));
          var dialog = listContainer.$.dialog.get();

          registerMessageCallback('removeVisits', this, function() {
            PolymerTest.flushTasks().then(function() {
              deleteComplete();
              return waitForEvent(groupedList, 'dom-change', function() {
                return polymerSelectAll(
                    groupedList, '.dropdown-indicator').length == 1;
              });
            }).then(function() {
              items = polymerSelectAll(groupedList, 'history-item');
              assertEquals(2, items.length);
              assertEquals('https://en.wikipedia.org/b', items[0].item.title);
              assertEquals('https://en.wikipedia.org/d', items[1].item.title);
              assertEquals(
                  1, polymerSelectAll(groupedList, '.domain-heading')
                         .length);

              assertEquals(
                  1, polymerSelectAll(groupedList,
                                             '.group-container').length);

              assertFalse(dialog.open);
              done();
            });
          });
          // Confirmation dialog should appear.
          assertTrue(dialog.open);

          MockInteractions.tap(listContainer.$$('.action-button'));
        });
      });

      test('build removal tree', function() {
        var paths = [
          'a.0.b.1',
          'a.0.b.3',
          'a.2.b.3',
        ];

        var expected = {
          currentPath: 'a',
          leaf: false,
          indexes: [0, 2],
          children: [
            {
              currentPath: 'a.0.b',
              leaf: true,
              indexes: [1, 3],
              children: [],
            },
            null,
            {
              currentPath: 'a.2.b',
              leaf: true,
              indexes: [3],
              children: [],
            },
          ],
        };

        assertEquals(
            JSON.stringify(expected),
            JSON.stringify(groupedList.buildRemovalTree_(paths)));
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
