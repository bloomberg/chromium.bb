// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_toolbar_test', function() {
  // Array of test history data.
  var TEST_HISTORY_RESULTS = [
    {
      "dateRelativeDay": "Today - Wednesday, December 9, 2015",
      "url": "https://www.google.com"
    }
  ];

  function registerTests() {
    suite('history-toolbar', function() {
      var element;
      var toolbar;

      suiteSetup(function() {
        element = $('history-list');
        toolbar = $('toolbar');
      });

      test('selecting checkbox causes toolbar to change', function(done) {
        element.addNewResults(TEST_HISTORY_RESULTS);

        flush(function() {
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

          done();
        });
      });

      teardown(function() {
        toolbar.count = 0;
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
