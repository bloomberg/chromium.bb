// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-settings-checkbox. */
cr.define('cr_settings_checkbox', function() {
  function registerTests() {
    suite('CrSettingsCheckbox', function() {
      /**
       * Checkbox created before each test.
       * @type {CrSettingsCheckbox}
       */
      var checkbox;

      /**
       * Pref value used in tests, should reflect checkbox 'checked' attribute.
       * @type {CrSettingsCheckbox}
       */
      var pref = {
        key: 'test',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true
      };

      // Import cr_settings_checkbox.html before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml(
            'chrome://md-settings/checkbox/checkbox.html');
      });

      // Initialize a checked cr-settings-checkbox before each test.
      setup(function() {
        PolymerTest.clearBody();
        checkbox = document.createElement('cr-settings-checkbox');
        checkbox.set('pref', pref);
        document.body.appendChild(checkbox);
      });

      test('responds to checked attribute', function() {
        assertTrue(checkbox.checked);

        checkbox.removeAttribute('checked');
        assertFalse(checkbox.checked);
        assertFalse(pref.value);

        checkbox.setAttribute('checked', '');
        assertTrue(checkbox.checked);
        assertTrue(pref.value);
      });

      test('fires a change event', function(done) {
        checkbox.addEventListener('change', function() {
          assertFalse(checkbox.checked);
          done();
        });
        MockInteractions.tap(checkbox.$.checkbox);
      });

      test('does not change when disabled', function() {
        checkbox.checked = false;
        checkbox.setAttribute('disabled', '');
        assertTrue(checkbox.disabled);
        assertTrue(checkbox.$.checkbox.disabled);

        MockInteractions.tap(checkbox.$.checkbox);
        assertFalse(checkbox.checked);
        assertFalse(checkbox.$.checkbox.checked);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
