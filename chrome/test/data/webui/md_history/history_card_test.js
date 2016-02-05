// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_card_test', function() {
  function registerTests() {
    suite('history-card', function() {
      var element;

      suiteSetup(function() {
        element = document.createElement('history-card');
        element.historyItems = [
          {"time": "1000000000"},
          {"time": "10000000"},
          {"time": "900000"}
        ];
      })

      test('basic separator insertion', function(done) {
        flush(function() {
          // Check that the correct number of time gaps are inserted.
          var spacers =
              Polymer.dom(element.root).querySelectorAll('#time-gap-separator');
          assertEquals(2, spacers.length);
          done();
        });
      });

      test('separator insertion when items change but item list length stays ' +
          'the same', function(done) {
        element.set('historyItems', [{"time": "900000"},
                                     {"time": "900000"},
                                     {"time": "900000"}]);

        flush(function() {
          var items =
            Polymer.dom(element.root).querySelectorAll('history-item');
          var spacers =
            Polymer.dom(element.root).querySelectorAll('#time-gap-separator');

          assertEquals('900000', items[0].timestamp_);
          assertEquals('900000', items[1].timestamp_);
          assertEquals('900000', items[2].timestamp_);

          // Note that the spacers aren't actually removed, are just set to:
          // display: none;
          for (var i = 0; i < spacers.length; i++) {
            assertEquals(spacers[i].style.display, 'none');
          }
          done();
        });
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
