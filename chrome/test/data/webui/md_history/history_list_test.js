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

      test('cancelling selection of multiple items', function() {
        app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
        return flush().then(function() {
          var items =
              Polymer.dom(element.root).querySelectorAll('history-item');

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
        app.queryState_.queryingDisabled = false;
        registerMessageCallback('queryHistory', this, function (info) {
          assertEquals('example.com', info[0]);
          flush().then(function() {
            assertEquals(
                'example.com',
                toolbar.$['main-toolbar'].getSearchField().getValue());
            done();
          });
        });

        app.$['history'].$.sharedMenu.itemData = {domain: 'example.com'};
        MockInteractions.tap(app.$['history'].$.menuMoreButton);
      });

      test('scrolling history list closes overflow menu', function() {
        var sharedMenu = app.$.history.$.sharedMenu;
        for (var i = 0; i < 10; i++)
          app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);

        return flush().then(function() {
          items = Polymer.dom(element.root).querySelectorAll('history-item');

          MockInteractions.tap(items[2].$['menu-button']);
          assertTrue(sharedMenu.menuOpen);
          element.$['infinite-list'].scrollTop = 100;
          return flush();
        }).then(function() {
          assertFalse(sharedMenu.menuOpen);
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
        flush().then(function() {
          items = Polymer.dom(element.root).querySelectorAll('history-item');

          MockInteractions.tap(items[2].$.checkbox);
          MockInteractions.tap(items[5].$.checkbox);
          MockInteractions.tap(items[7].$.checkbox);

          registerMessageCallback('removeVisits', this, function() {
            flush().then(function() {
              deleteComplete();
            }).then(function() {
              assertEquals(element.historyData_.length, 5);
              assertEquals(element.historyData_[0].dateRelativeDay,
                           '2016-03-15');
              assertEquals(element.historyData_[2].dateRelativeDay,
                           '2016-03-13');
              assertEquals(element.historyData_[4].dateRelativeDay,
                           '2016-03-11');
              assertFalse(listContainer.$.dialog.open);
              done();
            });
          });

          MockInteractions.tap(app.$.toolbar.$$('#delete-button'));

          // Confirmation dialog should appear.
          assertTrue(listContainer.$.dialog.open);

          MockInteractions.tap(listContainer.$$('.action-button'));
        });
      });

      test('deleting items using shortcuts', function(done) {
        var listContainer = app.$.history;
        app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
        flush().then(function() {
          items = Polymer.dom(element.root).querySelectorAll('history-item');

          // Dialog should not appear when there is no item selected.
          MockInteractions.pressAndReleaseKeyOn(
            document.body, 46, '', 'Delete');
          assertFalse(listContainer.$.dialog.open);

          MockInteractions.tap(items[1].$.checkbox);
          MockInteractions.tap(items[2].$.checkbox);

          assertEquals(2, toolbar.count);

          registerMessageCallback('removeVisits', this, function(toRemove) {
            assertEquals('https://www.example.com', toRemove[0].url);
            assertEquals('https://www.google.com', toRemove[1].url);
            assertEquals('2016-03-14 10:00 UTC', toRemove[0].timestamps[0]);
            assertEquals('2016-03-14 9:00 UTC', toRemove[1].timestamps[0]);
            done();
          });

          MockInteractions.pressAndReleaseKeyOn(
            document.body, 46, '', 'Delete');
          assertTrue(listContainer.$.dialog.open);

          MockInteractions.tap(listContainer.$$('.cancel-button'));
          assertFalse(listContainer.$.dialog.open);

          MockInteractions.pressAndReleaseKeyOn(
            document.body, 8, '', 'Backspace');
          assertTrue(listContainer.$.dialog.open);

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
          MockInteractions.tap(app.$.toolbar.$$('#delete-button'));

          // Confirmation dialog should appear.
          assertTrue(listContainer.$.dialog.open);

          app.set('queryState_.searchTerm', 'something else');
          assertFalse(listContainer.$.dialog.open);
        });
      });

      teardown(function() {
        element.historyData_ = [];
        registerMessageCallback('removeVisits', this, undefined);
        registerMessageCallback('queryHistory', this, function() {});
        app.queryState_.queryingDisabled = true;
        app.set('queryState_.searchTerm', '');
        return flush();
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
