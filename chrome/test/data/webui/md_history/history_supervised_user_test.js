// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_supervised_user_test', function() {
  // Array of test history data.
  var TEST_HISTORY_RESULTS = [
    {
      "dateRelativeDay": "Today - Wednesday, December 9, 2015",
      "url": "https://www.google.com"
    }
  ];

  function registerTests() {
    suite('history-card-manager supervised-user', function() {
      var element;

      suiteSetup(function() {
        element = $('history-card-manager');
      });

      test('checkboxes disabled for supervised user', function(done) {
        element.addNewResults(TEST_HISTORY_RESULTS);
        var toolbar = $('toolbar');

        flush(function() {
          var card = Polymer.dom(element.root).querySelectorAll('history-card');
          var item = Polymer.dom(card[0].root).querySelectorAll('history-item');

          MockInteractions.tap(item[0].$['checkbox']);

          assertFalse(item[0].selected);
          done();
        });
      });

      test('deletion disabled for supervised user', function() {
        element.addNewResults(TEST_HISTORY_RESULTS);

        // Make sure that removeVisits is not being called.
        registerMessageCallback('removeVisits', this, function (toBeRemoved) {
          assertTrue(false);
        });

        element.historyDataByDay_[0].historyItems[0].selected = true;
        $('toolbar').onDeleteTap_();
      });

      teardown(function() {
        element.historyDataByDay_ = [];
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
