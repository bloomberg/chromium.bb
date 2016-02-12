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
    suite('history-list supervised-user', function() {
      var element;

      suiteSetup(function() {
        element = $('history-list');
      });

      setup(function() {
        element.addNewResults(TEST_HISTORY_RESULTS);
      });

      test('checkboxes disabled for supervised user', function(done) {
        var toolbar = $('toolbar');

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
        $('toolbar').onDeleteTap_();
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
