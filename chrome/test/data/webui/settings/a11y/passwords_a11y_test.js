// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Define accessibility tests for the MANAGE_PASSWORDS route. */

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  'settings_accessibility_test.js',
]);

/**
 * Test fixture for MANAGE PASSWORDS
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsA11yManagePasswords() {}

SettingsA11yManagePasswords.prototype = {
  __proto__: SettingsAccessibilityTest.prototype,

  // Include files that define the mocha tests.
  extraLibraries: SettingsAccessibilityTest.prototype.extraLibraries.concat([
    '../passwords_and_autofill_fake_data.js',
  ]),
};

AccessibilityTest.define('SettingsA11yManagePasswords', {
  /** @override */
  name: 'MANAGE_PASSWORDS',
  /** @type {PasswordManager} */
  passwordManager: null,
  /** @type {PasswordsSectionElement}*/
  passwordsSection: null,
  axeOptions: {
    'rules': {
      // TODO(hcarmona): enable 'region' after addressing violation.
      'region': {enabled: false},
      // Disable 'skip-link' check since there are few tab stops before the main
      // content.
      'skip-link': {enabled: false},
      // Disable rules flaky for CFI build.
      'meta-viewport': {enabled: false},
      'list': {enabled: false},
      'frame-title': {enabled: false},
      'label': {enabled: false},
      'hidden-content': {enabled: false},
      'aria-valid-attr-value': {enabled: false},
      'button-name': {enabled: false},
    }
  },
  /** @override */
  setup: function() {
    return new Promise((resolve) => {
      // Reset to a blank page.
      PolymerTest.clearBody();

      // Set the URL to be that of specific route to load upon injecting
      // settings-ui. Simply calling settings.navigateTo(route) prevents
      // use of mock APIs for fake data.
      window.history.pushState(
          'object or string', 'Test', settings.routes.MANAGE_PASSWORDS.path);

      PasswordManagerImpl.instance_ = new TestPasswordManager();
      this.passwordManager = PasswordManagerImpl.instance_;

      var settingsUi = document.createElement('settings-ui');

      // The settings section will expand to load the MANAGE_PASSWORDS route
      // (based on the URL set above) once the settings-ui element is attached
      settingsUi.addEventListener('settings-section-expanded', () => {
        // Passwords section should be loaded before setup is complete.
        this.passwordsSection = settingsUi.$$('settings-main')
                                    .$$('settings-basic-page')
                                    .$$('settings-passwords-and-forms-page')
                                    .$$('passwords-section');
        assertTrue(!!this.passwordsSection);

        assertEquals(
            this.passwordManager, this.passwordsSection.passwordManager_);

        resolve();
      });

      document.body.appendChild(settingsUi);
    });
  },

  /** @override */
  tests: {
    'Accessible with 0 passwords': function() {
      assertEquals(0, this.passwordsSection.savedPasswords.length);
    },
    'Accessible with 10 passwords': function() {
      var fakePasswords = [];
      for (var i = 0; i < 10; i++) {
        fakePasswords.push(FakeDataMaker.passwordEntry());
      }
      // Set list of passwords.
      this.passwordManager.lastCallback.addSavedPasswordListChangedListener(
          fakePasswords);
      Polymer.dom.flush();

      assertEquals(10, this.passwordsSection.savedPasswords.length);
    },
  },
  /** @override */
  violationFilter:
      Object.assign({}, SettingsAccessibilityTest.violationFilter, {
        'button-name': function(nodeResult) {
          var node = nodeResult.element;
          return node.classList.contains('icon-expand-more');
        },
      }),
});
