// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_card_test', function() {
  function registerTests() {
    suite('history-card', function() {
      test('basic separator insertion', function(done) {
        var element = document.createElement('history-card');
        element.historyItems = [
          {"time": "1000000000"},
          {"time": "10000000"},
          {"time": "900000"}
        ];
        flush(function() {
          // Check that the correct number of time gaps are inserted.
          var spacers =
              Polymer.dom(element.root).querySelectorAll('#time-gap-separator');
          assertEquals(2, spacers.length);
          done();
        });
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
