// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for settings-checkbox. */
cr.define('settings_checkbox', function() {
  function registerTests() {
    suite('SettingsCheckbox', function() {
      /**
       * Checkbox created before each test.
       * @type {SettingsCheckbox}
       */
      var testElement;

      /**
       * Pref value used in tests, should reflect checkbox 'checked' attribute.
       * @type {SettingsCheckbox}
       */
      var pref = {
        key: 'test',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true
      };

      // Import settings_checkbox.html before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml(
            'chrome://md-settings/controls/settings_checkbox.html');
      });

      // Initialize a checked settings-checkbox before each test.
      setup(function() {
        PolymerTest.clearBody();
        testElement = document.createElement('settings-checkbox');
        testElement.set('pref', pref);
        document.body.appendChild(testElement);
      });

      test('responds to checked attribute', function() {
        assertTrue(testElement.checked);

        testElement.removeAttribute('checked');
        assertFalse(testElement.checked);
        assertFalse(pref.value);

        testElement.setAttribute('checked', '');
        assertTrue(testElement.checked);
        assertTrue(pref.value);
      });

      test('fires a change event', function(done) {
        testElement.addEventListener('change', function() {
          assertFalse(testElement.checked);
          done();
        });
        MockInteractions.tap(testElement.$.checkbox);
      });

      test('does not change when disabled', function() {
        testElement.checked = false;
        testElement.setAttribute('disabled', '');
        assertTrue(testElement.disabled);
        assertTrue(testElement.$.checkbox.disabled);

        MockInteractions.tap(testElement.$.checkbox);
        assertFalse(testElement.checked);
        assertFalse(testElement.$.checkbox.checked);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
