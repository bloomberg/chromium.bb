// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of accessibility tests for the passwords page. */

suite('SettingsPasswordsAccessibility', function() {
  var passwordsSection = null;
  var passwordManager = null;

  setup(function() {
    return new Promise(function(resolve) {
      // Reset the blank to be a blank page.
      PolymerTest.clearBody();

      // Set the URL to be that of specific route to load upon injecting
      // settings-ui. Simply calling settings.navigateTo(route) prevents
      // use of mock APIs for fake data.
      window.history.pushState(
          'object or string', 'Test', settings.routes.MANAGE_PASSWORDS.path);

      PasswordManagerImpl.instance_ = new TestPasswordManager();
      passwordManager = PasswordManagerImpl.instance_;

      var settingsUi = document.createElement('settings-ui');

      // The settings section will expand to load the MANAGE_PASSWORDS route
      // (based on the URL set above) once the settings-ui element is attached
      // to the DOM.
      settingsUi.addEventListener('settings-section-expanded', function () {

        // Passwords section should be loaded before setup is complete.
        passwordsSection = document.querySelector('* /deep/ passwords-section');
        assertTrue(!!passwordsSection);

        assertEquals(passwordManager, passwordsSection.passwordManager_);

        resolve();
      });

      document.body.appendChild(settingsUi);
    });
  });

  test('Accessible with 0 passwords', function() {
    assertEquals(passwordsSection.savedPasswords.length, 0);
    return SettingsAccessibilityTest.runAudit();
  });

  test('Accessible with 100 passwords', function() {
    var fakePasswords = [];
    for (var i = 0; i < 100; i++) {
      fakePasswords.push(FakeDataMaker.passwordEntry());
    }

    // Set list of passwords.
    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        fakePasswords);
    Polymer.dom.flush();

    assertEquals(100, passwordsSection.savedPasswords.length);

    return SettingsAccessibilityTest.runAudit();
  });
});