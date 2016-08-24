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
        element = app.$['history'].$['infinite-list'];
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

      setup(function() {
        app.queryState_.incremental = true;
      });

      test('deleting single item', function(done) {
        var listContainer = app.$.history;
        app.historyResult(createHistoryInfo(), [
          createHistoryEntry('2015-01-01', 'http://example.com')
        ]);

        flush().then(function() {
          assertEquals(element.historyData_.length, 1);
          items = polymerSelectAll(element, 'history-item');
          MockInteractions.tap(items[0].$.checkbox);
          assertDeepEquals([true],
                           element.historyData_.map(i => i.selected));
          return flush();
        }).then(function() {
          MockInteractions.tap(app.$.toolbar.$$('#delete-button'));
          return listContainer.$.dialog.get();
        }).then(function(dialog) {
          registerMessageCallback('removeVisits', this, function() {
            flush().then(function() {
              deleteComplete();
              return flush();
            }).then(function() {
              items = polymerSelectAll(element, 'history-item');
              assertEquals(element.historyData_.length, 0);
              done();
            });
          });
          assertTrue(dialog.open);
          MockInteractions.tap(listContainer.$$('.action-button'));
        });
      });

      test('cancelling selection of multiple items', function() {
        app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
        return flush().then(function() {
          var items = polymerSelectAll(element, 'history-item');

          MockInteractions.tap(items[2].$.checkbox);
          MockInteractions.tap(items[3].$.checkbox);

          // Make sure that the array of data that determines whether or not an
          // item is selected is what we expect after selecting the two items.
          assertDeepEquals([false, false, true, true],
                           element.historyData_.map(i => i.selected));

          toolbar.onClearSelectionTap_();

          // Make sure that clearing the selection updates both the array and
          // the actual history-items affected.
          assertDeepEquals([false, false, false, false],
                           element.historyData_.map(i => i.selected));

          assertFalse(items[2].$.checkbox.checked);
          assertFalse(items[3].$.checkbox.checked);
        });
      });

      test('selection of multiple items using shift click', function() {
        app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
        return flush().then(function() {
          var items = polymerSelectAll(element, 'history-item');

          MockInteractions.tap(items[1].$.checkbox);
          assertDeepEquals([false, true, false, false],
                           element.historyData_.map(i => i.selected));
          assertDeepEquals(
              ['historyData_.1'],
              Array.from(element.selectedPaths).sort());

          // Shift-select to the last item.
          shiftClick(items[3].$.checkbox);
          assertDeepEquals([false, true, true, true],
                           element.historyData_.map(i => i.selected));
          assertDeepEquals(
              ['historyData_.1', 'historyData_.2', 'historyData_.3'],
              Array.from(element.selectedPaths).sort());

          // Shift-select back to the first item.
          shiftClick(items[0].$.checkbox);
          assertDeepEquals([true, true, true, true],
                           element.historyData_.map(i => i.selected));
          assertDeepEquals(
              [
                'historyData_.0', 'historyData_.1', 'historyData_.2',
                'historyData_.3'
              ],
              Array.from(element.selectedPaths).sort());

          // Shift-deselect to the third item.
          shiftClick(items[2].$.checkbox);
          assertDeepEquals([false, false, false, true],
                           element.historyData_.map(i => i.selected));
          assertDeepEquals(
              ['historyData_.3'],
              Array.from(element.selectedPaths).sort());

          // Select the second item.
          MockInteractions.tap(items[1].$.checkbox);
          assertDeepEquals([false, true, false, true],
                           element.historyData_.map(i => i.selected));
          assertDeepEquals(
              ['historyData_.1', 'historyData_.3'],
              Array.from(element.selectedPaths).sort());

          // Shift-deselect to the last item.
          shiftClick(items[3].$.checkbox);
          assertDeepEquals([false, false, false, false],
                           element.historyData_.map(i => i.selected));
          assertDeepEquals(
              [],
              Array.from(element.selectedPaths).sort());

          // Shift-select back to the third item.
          shiftClick(items[2].$.checkbox);
          assertDeepEquals([false, false, true, true],
                           element.historyData_.map(i => i.selected));
          assertDeepEquals(
              ['historyData_.2', 'historyData_.3'],
              Array.from(element.selectedPaths).sort());

          // Remove selected items.
          element.removeItemsByPath(Array.from(element.selectedPaths));
          assertDeepEquals(
              ['https://www.google.com', 'https://www.example.com'],
              element.historyData_.map(i => i.title));
        });
      });

      test('setting first and last items', function() {
        app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);

        return flush().then(function() {
          var items = polymerSelectAll(element, 'history-item');
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
              polymerSelectAll(element, 'history-item');
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

          element.removeItemsByPath([
            'historyData_.2', 'historyData_.5', 'historyData_.7'
          ]);

          return flush();
        }).then(function() {
          items = polymerSelectAll(element, 'history-item');

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
          assertTrue(items[4].isCardEnd);
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
        app.queryState_.queryingDisabled = false;
        flush().then(function () {
          registerMessageCallback('queryHistory', this, function (info) {
            assertEquals('www.google.com', info[0]);
            flush().then(function() {
              assertEquals(
                  'www.google.com',
                  toolbar.$['main-toolbar'].getSearchField().getValue());
              done();
            });
          });

          items = polymerSelectAll(element, 'history-item');
          MockInteractions.tap(items[0].$['menu-button']);
          return app.$.history.$.sharedMenu.get();
        }).then(function() {
          MockInteractions.tap(app.$.history.$$('#menuMoreButton'));
        });
      });

      test('scrolling history list closes overflow menu', function() {
        var lazyMenu = app.$.history.$.sharedMenu;
        for (var i = 0; i < 10; i++)
          app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
        return flush().then(function() {
          items = polymerSelectAll(element, 'history-item');

          MockInteractions.tap(items[2].$['menu-button']);
          return flush();
        }).then(function() {
          assertTrue(lazyMenu.getIfExists().menuOpen);
          element.$['infinite-list'].scrollToIndex(20);
          return waitForEvent(lazyMenu.getIfExists(), 'menu-open-changed');
        }).then(function() {
          assertFalse(lazyMenu.getIfExists().menuOpen);
        });
      });

      test('scrolling history list causes toolbar shadow to appear', () => {
        for (var i = 0; i < 10; i++)
          app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
        return flush().then(function() {
          assertFalse(app.toolbarShadow_);
          element.$['infinite-list'].scrollToIndex(20);
          return waitForEvent(app, 'toolbar-shadow_-changed');
        }).then(() => {
          assertTrue(app.toolbarShadow_);
          element.$['infinite-list'].scrollToIndex(0);
          return waitForEvent(app, 'toolbar-shadow_-changed');
        }).then(() => {
          assertFalse(app.toolbarShadow_);
        });
      });

      test('changing search deselects items', function() {
        app.historyResult(
            createHistoryInfo('ex'),
            [createHistoryEntry('2016-06-9', 'https://www.example.com')]);
        return flush().then(function() {
          var item = element.$$('history-item');
          MockInteractions.tap(item.$.checkbox);

          assertEquals(1, toolbar.count);
          app.queryState_.incremental = false;

          app.historyResult(
              createHistoryInfo('ample'),
              [createHistoryEntry('2016-06-9', 'https://www.example.com')]);
          assertEquals(0, toolbar.count);
        });
      });

      test('delete items end to end', function(done) {
        var listContainer = app.$.history;
        app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
        app.historyResult(createHistoryInfo(), ADDITIONAL_RESULTS);
        app.historyResult(createHistoryInfo(), [
          createHistoryEntry('2015-01-01', 'http://example.com'),
          createHistoryEntry('2015-01-01', 'http://example.com'),
          createHistoryEntry('2015-01-01', 'http://example.com')
        ]);
        flush().then(function() {
          items = polymerSelectAll(element, 'history-item');

          MockInteractions.tap(items[2].$.checkbox);
          MockInteractions.tap(items[5].$.checkbox);
          MockInteractions.tap(items[7].$.checkbox);
          MockInteractions.tap(items[8].$.checkbox);
          MockInteractions.tap(items[9].$.checkbox);
          MockInteractions.tap(items[10].$.checkbox);

          return flush();
        }).then(function() {
          MockInteractions.tap(app.$.toolbar.$$('#delete-button'));

          return listContainer.$.dialog.get();
        }).then(function(dialog) {
          registerMessageCallback('removeVisits', this, function() {
            flush().then(function() {
              deleteComplete();
              return flush();
            }).then(function() {
              assertEquals(element.historyData_.length, 5);
              assertEquals(element.historyData_[0].dateRelativeDay,
                           '2016-03-15');
              assertEquals(element.historyData_[2].dateRelativeDay,
                           '2016-03-13');
              assertEquals(element.historyData_[4].dateRelativeDay,
                           '2016-03-11');
              assertFalse(dialog.open);

              // Ensure the UI is correctly updated.
              items = polymerSelectAll(element, 'history-item');
              assertEquals('https://www.google.com', items[0].item.title);
              assertEquals('https://www.example.com', items[1].item.title);
              assertEquals('https://en.wikipedia.org', items[2].item.title);
              assertEquals('https://en.wikipedia.org', items[3].item.title);
              assertEquals('https://www.google.com', items[4].item.title);

              assertFalse(dialog.open);
              done();
            });
          });

          // Confirmation dialog should appear.
          assertTrue(dialog.open);
          MockInteractions.tap(listContainer.$$('.action-button'));
        });
      });

      test('delete via menu button', function(done) {
        var listContainer = app.$.history;
        app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);

        flush().then(function() {
          items = polymerSelectAll(element, 'history-item');
          registerMessageCallback('removeVisits', this, function() {
            flush().then(function() {
              deleteComplete();
              return flush();
            }).then(function() {
              assertDeepEquals([
                'https://www.google.com',
                'https://www.google.com',
                'https://en.wikipedia.org',
              ], element.historyData_.map(item => item.title));
              done();
            });
          });
          MockInteractions.tap(items[1].$['menu-button']);
          return app.$.history.$.sharedMenu.get();
        }).then(function(menu) {
          MockInteractions.tap(app.$.history.$$('#menuRemoveButton'));
        });
      });

      test('deleting items using shortcuts', function(done) {
        var listContainer = app.$.history;
        var dialog;
        app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
        listContainer.$.dialog.get().then(function(stampedDialog) {
          dialog = stampedDialog;
          return flush();
        }).then(function() {
          items = polymerSelectAll(element, 'history-item');

          // Dialog should not appear when there is no item selected.
          MockInteractions.pressAndReleaseKeyOn(
            document.body, 46, '', 'Delete');
          return flush();
        }).then(function() {
          assertFalse(dialog.open);

          MockInteractions.tap(items[1].$.checkbox);
          MockInteractions.tap(items[2].$.checkbox);

          assertEquals(2, toolbar.count);

          MockInteractions.pressAndReleaseKeyOn(
            document.body, 46, '', 'Delete');
          return flush();
        }).then(function() {
          assertTrue(dialog.open);
          MockInteractions.tap(listContainer.$$('.cancel-button'));
          assertFalse(dialog.open);

          MockInteractions.pressAndReleaseKeyOn(
            document.body, 8, '', 'Backspace');
          return flush();
        }).then(function() {
          assertTrue(dialog.open);

          registerMessageCallback('removeVisits', this, function(toRemove) {
            assertEquals('https://www.example.com', toRemove[0].url);
            assertEquals('https://www.google.com', toRemove[1].url);
            assertEquals('2016-03-14 10:00 UTC', toRemove[0].timestamps[0]);
            assertEquals('2016-03-14 9:00 UTC', toRemove[1].timestamps[0]);
            done();
          });

          MockInteractions.tap(listContainer.$$('.action-button'));
        });
      });

      test('delete dialog closed on url change', function() {
        app.queryState_.queryingDisabled = false;
        var listContainer = app.$.history;
        app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
        app.historyResult(createHistoryInfo(), ADDITIONAL_RESULTS);
        return flush().then(function() {
          items = Polymer.dom(element.root).querySelectorAll('history-item');

          MockInteractions.tap(items[2].$.checkbox);
          return flush();
        }).then(function() {
          MockInteractions.tap(app.$.toolbar.$$('#delete-button'));
          return flush();
        }).then(function() {
          // Confirmation dialog should appear.
          assertTrue(listContainer.$.dialog.getIfExists().open);

          app.set('queryState_.searchTerm', 'something else');
          assertFalse(listContainer.$.dialog.getIfExists().open);
        });
      });

      test('clicking file:// url sends message to chrome', function(done) {
        var fileURL = 'file:///home/myfile';
        app.historyResult(createHistoryInfo(), [
          createHistoryEntry('2016-03-15', fileURL),
        ]);
        flush().then(function() {
          var items =
              Polymer.dom(element.root).querySelectorAll('history-item');

          registerMessageCallback('navigateToUrl', this, function(info) {
            assertEquals(fileURL, info[0]);
            done();
          });

          MockInteractions.tap(items[0].$.title);
        });
      });

      teardown(function() {
        element.historyData_ = [];
        registerMessageCallback('removeVisits', this, undefined);
        registerMessageCallback('queryHistory', this, function() {});
        registerMessageCallback('navigateToUrl', this, undefined);
        app.queryState_.queryingDisabled = true;
        app.set('queryState_.searchTerm', '');
        // Unselect all items, clear the list.
        element.addNewResults([], false);
        return flush();
      });
    });
  }

  return {
    registerTests: registerTests
  };
});
