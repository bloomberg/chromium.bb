// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_toolbar_test', function() {
  function registerTests() {
    suite('history-toolbar', function() {
      var app;
      var element;
      var toolbar;
      var TEST_HISTORY_RESULTS;

      suiteSetup(function() {
        TEST_HISTORY_RESULTS =
            [createHistoryEntry('2016-03-15', 'https://google.com')];
      });

      setup(function() {
        app = replaceApp();
        element = app.$['history'].$['infinite-list'];
        toolbar = app.$['toolbar'];
        return PolymerTest.flushTasks();
      });

      test('selecting checkbox causes toolbar to change', function() {
        element.addNewResults(TEST_HISTORY_RESULTS);

        return PolymerTest.flushTasks().then(function() {
          var item = element.$$('history-item');
          MockInteractions.tap(item.$.checkbox);

          // Ensure that when an item is selected that the count held by the
          // toolbar increases.
          assertEquals(1, toolbar.count);
          // Ensure that the toolbar boolean states that at least one item is
          // selected.
          assertTrue(toolbar.itemsSelected_);

          MockInteractions.tap(item.$.checkbox);

          // Ensure that when an item is deselected the count held by the
          // toolbar decreases.
          assertEquals(0, toolbar.count);
          // Ensure that the toolbar boolean states that no items are selected.
          assertFalse(toolbar.itemsSelected_);
        });
      });

      test('search term gathered correctly from toolbar', function(done) {
        app.queryState_.queryingDisabled = false;
        registerMessageCallback('queryHistory', this, function (info) {
          assertEquals('Test', info[0]);
          done();
        });

        toolbar.$$('cr-toolbar').fire('search-changed', 'Test');
      });

      test('shortcuts to open search field', function() {
        var field = toolbar.$['main-toolbar'].getSearchField();
        field.blur();
        assertFalse(field.showingSearch);

        MockInteractions.pressAndReleaseKeyOn(
            document.body, 191, '', '/');
        assertTrue(field.showingSearch);
        assertEquals(field.$.searchInput, field.root.activeElement);

        MockInteractions.pressAndReleaseKeyOn(
            field.$.searchInput, 27, '', 'Escape');
        assertFalse(field.showingSearch, 'Pressing escape closes field.');
        assertNotEquals(field.$.searchInput, field.root.activeElement);

        var modifier = 'ctrl';
        if (cr.isMac)
          modifier = 'meta';

        MockInteractions.pressAndReleaseKeyOn(
            document.body, 70, modifier, 'f');
        assertTrue(field.showingSearch);
        assertEquals(field.$.searchInput, field.root.activeElement);
      });

      test('spinner is active on search' , function(done) {
        app.queryState_.queryingDisabled = false;
        registerMessageCallback('queryHistory', this, function (info) {
          assertTrue(toolbar.spinnerActive);
          app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
          assertFalse(toolbar.spinnerActive);
          done();
        });

        toolbar.$$('cr-toolbar').fire('search-changed', 'Test2');
      });

      test('grouped history navigation buttons', function() {
        var info = createHistoryInfo();
        info.finished = false;
        app.historyResult(info, []);
        app.grouped_ = true;
        return PolymerTest.flushTasks().then(function() {
          app.set('queryState_.range', HistoryRange.MONTH);
          groupedList = app.$.history.$$('#grouped-list');
          assertTrue(!!groupedList);
          var today = toolbar.$$('#today-button');
          var next = toolbar.$$('#next-button');
          var prev = toolbar.$$('#prev-button');

          assertEquals(0, toolbar.groupedOffset);
          assertTrue(today.disabled);
          assertTrue(next.disabled);
          assertFalse(prev.disabled);

          MockInteractions.tap(prev);
          assertEquals(1, toolbar.groupedOffset);
          assertFalse(today.disabled);
          assertFalse(next.disabled);
          assertFalse(prev.disabled);

          app.historyResult(createHistoryInfo(), []);
          assertFalse(today.disabled);
          assertFalse(next.disabled);
          assertTrue(prev.disabled);
        });
      });

      teardown(function() {
        registerMessageCallback('queryHistory', this, function() {});
      });
    });
  }
  return {
    registerTests: registerTests
  };
});


cr.define('md_history.history_toolbar_focus_test', function() {
  function registerTests() {
    suite('history-toolbar', function() {
      var app;
      var element;
      var toolbar;
      var TEST_HISTORY_RESULTS =
          [createHistoryEntry('2016-03-15', 'https://google.com')];
      ;

      setup(function() {
        window.resultsRendered = false;
        app = replaceApp();

        element = app.$['history'].$['infinite-list'];
        toolbar = app.$['toolbar'];
      });

      test('search bar is focused on load in wide mode', function() {
        toolbar.$['main-toolbar'].narrow_ = false;

        historyResult(createHistoryInfo(), []);
        return PolymerTest.flushTasks().then(() => {
          // Ensure the search bar is focused on load.
          assertTrue(
              app.$.toolbar.$['main-toolbar']
                  .getSearchField()
                  .isSearchFocused());
        });
      });

      test('search bar is not focused on load in narrow mode', function() {
        toolbar.$['main-toolbar'].narrow_ = true;

        historyResult(createHistoryInfo(), []);
        return PolymerTest.flushTasks().then(() => {
          // Ensure the search bar is focused on load.
          assertFalse(
              $('history-app')
                  .$.toolbar.$['main-toolbar']
                  .getSearchField()
                  .isSearchFocused());
        });
      });
    });
  };

  return {
    registerTests: registerTests
  };
});
