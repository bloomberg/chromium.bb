// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-kiosk-dialog. */
cr.define('extension_kiosk_mode_tests', function() {
  /** @enum {string} */
  let TestNames = {
    AddButton: 'AddButton',
  };

  let suiteName = 'kioskModeTests';

  suite(suiteName, function() {
    let browserProxy = null;
    let dialog = null;

    setup(function() {
      PolymerTest.clearBody();

      browserProxy = new TestKioskBrowserProxy();
      extensions.KioskBrowserProxyImpl.instance_ = browserProxy;

      dialog = document.createElement('extensions-kiosk-dialog');
      document.body.appendChild(dialog);

      Polymer.dom.flush();
    });

    test(assert(TestNames.AddButton), function() {
      const addButton = dialog.$['add-button'];
      expectTrue(!!addButton);
      expectTrue(addButton.disabled);

      const addInput = dialog.$['add-input'];
      addInput.value = 'blah';
      expectFalse(addButton.disabled);

      MockInteractions.tap(addButton);
      return browserProxy.whenCalled('addKioskApp').then(appId => {
        expectEquals(appId, 'blah');
      });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
