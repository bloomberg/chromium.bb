// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads.layout_tests', function() {
  function registerTests() {
    suite('LayoutTests', function() {
      /** @type {!downloads.Manager} */
      var manager;

      setup(function() {
        PolymerTest.clearBody();
        manager = document.createElement('downloads-manager');
        document.body.appendChild(manager);
      });

      test('long URLs ellide', function() {
        manager.updateAll_([{
          file_name: 'file name',
          state: downloads.States.COMPLETE,
          url: 'a'.repeat(10000),
        }]);

        Polymer.dom.flush();

        var item = manager.$$('downloads-item');
        assertLT(item.$$('#url').offsetWidth, item.offsetWidth);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
