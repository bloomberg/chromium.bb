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
        app = $('history-app');
        element = app.$['history-list'];
        toolbar = app.$['toolbar'];
        TEST_HISTORY_RESULTS =
            [createHistoryEntry('2016-03-15', 'https://google.com')];
      });

      test('selecting checkbox causes toolbar to change', function() {
        element.addNewResults(TEST_HISTORY_RESULTS);

        return flush().then(function() {
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
        app.queryingDisabled_ = false;
        registerMessageCallback('queryHistory', this, function (info) {
          assertEquals(info[0], 'Test');
          done();
        });

        toolbar.$$('cr-toolbar').fire('search-changed', 'Test');
      });

      teardown(function() {
        element.historyData_ = [];
        element.searchedTerm = '';
        registerMessageCallback('queryHistory', this, undefined);
        toolbar.count = 0;
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
