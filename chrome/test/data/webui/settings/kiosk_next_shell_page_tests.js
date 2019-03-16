// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('KioskNextShellPageTests', function() {
  /** @type {?SettingsKioskNextShellPageElement} */
  let page = null;

  /** @type {?settings.TestLifetimeBrowserProxy} */
  let lifetimeBrowserProxy = null;

  function setKioskNextShellPrefs(enabled) {
    page.prefs = {
      ash: {
        kiosk_next_shell: {
          enabled: {value: enabled},
        },
      },
    };
    Polymer.dom.flush();
  }

  function showDialog() {
    const dialogButton = page.$$('paper-button');
    assertTrue(!!dialogButton);
    dialogButton.click();
    Polymer.dom.flush();

    const dialog = page.$$('#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.$$('cr-dialog').open);
    return dialog;
  }

  setup(function() {
    lifetimeBrowserProxy = new settings.TestLifetimeBrowserProxy();
    settings.LifetimeBrowserProxyImpl.instance_ = lifetimeBrowserProxy;

    PolymerTest.clearBody();
    page = document.createElement('settings-kiosk-next-shell-page');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
    page = null;
  });

  test('kiosk next shell dialog can be canceled', function() {
    setKioskNextShellPrefs(false);
    const dialog = showDialog();

    const cancelButton = dialog.$$('#cancel');
    assertTrue(!!cancelButton);
    cancelButton.click();
    Polymer.dom.flush();

    // The value of the pref must still be the same and we shouldn't sign out.
    assertFalse(page.prefs.ash.kiosk_next_shell.enabled.value);
    assertEquals(0, lifetimeBrowserProxy.getCallCount('signOutAndRestart'));
  });

  test('turn on kiosk next shell', function() {
    setKioskNextShellPrefs(false);
    const dialog = showDialog();

    const confirmButton = dialog.$$('#confirm');
    assertTrue(!!confirmButton);
    confirmButton.click();

    assertTrue(page.prefs.ash.kiosk_next_shell.enabled.value);
    return lifetimeBrowserProxy.whenCalled('signOutAndRestart');
  });

  test('turn off kiosk next shell', function() {
    setKioskNextShellPrefs(true);
    const dialog = showDialog();

    const confirmButton = dialog.$$('#confirm');
    assertTrue(!!confirmButton);
    confirmButton.click();

    assertFalse(page.prefs.ash.kiosk_next_shell.enabled.value);
    return lifetimeBrowserProxy.whenCalled('signOutAndRestart');
  });
});
