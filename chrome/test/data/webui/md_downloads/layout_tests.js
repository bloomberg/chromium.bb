// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('layout tests', function() {
  /** @type {!downloads.Manager} */
  var manager;

  setup(function() {
    PolymerTest.clearBody();
    manager = document.createElement('downloads-manager');
    document.body.appendChild(manager);
    assertEquals(manager, downloads.Manager.get());
  });

  test('long URLs ellide', function() {
    downloads.Manager.insertItems(0, [{
      file_name: 'file name',
      state: downloads.States.COMPLETE,
      url: 'a'.repeat(1000),
    }]);

    Polymer.dom.flush();

    var item = manager.$$('downloads-item');
    assertLT(item.$$('#url').offsetWidth, item.offsetWidth);
    assertEquals(300, item.$$('#url').textContent.length);
  });
});
