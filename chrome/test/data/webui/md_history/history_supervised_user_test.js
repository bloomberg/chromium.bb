// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_supervised_user_test', function() {
  function registerTests() {
    suite('history-list supervised-user', function() {
      var element;
      var toolbar;
      var TEST_HISTORY_RESULTS;

      suiteSetup(function() {
        element = $('history-app').$['history-list'];
        toolbar = $('history-app').$['toolbar'];
        TEST_HISTORY_RESULTS =
            [createHistoryEntry('2016-03-15', 'https://www.google.com')];
      });

      setup(function() {
        element.addNewResults(TEST_HISTORY_RESULTS, '');
      });

      test('checkboxes disabled for supervised user', function(done) {
        flush(function() {
          var items =
              Polymer.dom(element.root).querySelectorAll('history-item');

          MockInteractions.tap(items[0].$['checkbox']);

          assertFalse(items[0].selected);
          done();
        });
      });

      test('deletion disabled for supervised user', function() {

        // Make sure that removeVisits is not being called.
        registerMessageCallback('removeVisits', this, function (toBeRemoved) {
          assertTrue(false);
        });

        element.historyData[0].selected = true;
        toolbar.onDeleteTap_();
      });

      teardown(function() {
        element.historyData = [];
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
