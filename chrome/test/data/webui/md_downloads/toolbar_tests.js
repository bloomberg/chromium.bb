// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('toolbar tests', function() {
  /** @type {!downloads.Toolbar} */
  var toolbar;

  setup(function() {
    toolbar = document.createElement('downloads-toolbar');
    document.body.appendChild(toolbar);
  });

  test('resize closes more options menu', function() {
    MockInteractions.tap(toolbar.$$('paper-icon-button'));
    assertTrue(toolbar.$.more.opened);

    window.dispatchEvent(new CustomEvent('resize'));
    assertFalse(toolbar.$.more.opened);
  });
});
