// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('sync-page-test', function() {
  /** @type {SyncPageElement} */ var testElement;

  setup(function() {
    PolymerTest.clearBody();

    testElement = document.createElement('settings-sync-page');
    document.body.appendChild(testElement);
  });

  test('autofocus correctly after container is shown', function() {
    cr.webUIListenerCallback('sync-prefs-changed', {passphraseRequired: true});
    Polymer.dom.flush();

    var input = testElement.$$('#existingPassphraseInput');

    var focused = false;
    input.addEventListener('focus', function() {
      focused = true;
    });

    // Simulate event normally fired by main_page_behavior after subpage
    // animation ends.
    testElement.fire('show-container');
    assertTrue(focused);
  });
});
