// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of accessibility tests for the passwords page. */

// Define a mocha suite for every route-rule combination.
for (let ruleId of AccessibilityAudit.ruleIds) {
  suite('MANAGE_PASSWORDS_' + ruleId, function() {
    var passwordsSection = null;
    var passwordManager = null;

    /** @type {AccessibilityAuditConfig} **/
    var auditOptions = {runOnly: {type: 'rule', values: [ruleId]}};

    setup(function() {
      return new Promise(function(resolve) {
        // Reset to a blank page.
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
        settingsUi.addEventListener('settings-section-expanded', function() {
          // Passwords section should be loaded before setup is complete.
          passwordsSection = settingsUi.$$('settings-main')
                                 .$$('settings-basic-page')
                                 .$$('settings-passwords-and-forms-page')
                                 .$$('passwords-section');

          assertTrue(!!passwordsSection);

          assertEquals(passwordManager, passwordsSection.passwordManager_);

          resolve();
        });

        document.body.appendChild(settingsUi);
      });
    });

    test('Accessible with 0 passwords', function() {
      assertEquals(0, passwordsSection.savedPasswords.length);
      return SettingsAccessibilityTest.runAudit(auditOptions);
    });

    test('Accessible with 10 passwords', function() {
      var fakePasswords = [];
      for (var i = 0; i < 10; i++) {
        fakePasswords.push(FakeDataMaker.passwordEntry());
      }

      // Set list of passwords.
      passwordManager.lastCallback.addSavedPasswordListChangedListener(
          fakePasswords);
      Polymer.dom.flush();

      assertEquals(10, passwordsSection.savedPasswords.length);

      return SettingsAccessibilityTest.runAudit(auditOptions);
    });
  });
};