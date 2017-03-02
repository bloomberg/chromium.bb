// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for settings-toggle-button. */
cr.define('settings_toggle_button', function() {
  function registerTests() {
    suite('SettingsToggleButton', function() {
      /**
       * Toggle button created before each test.
       * @type {SettingsCheckbox}
       */
      var testElement;

      // Initialize a checked control before each test.
      setup(function() {
        /**
         * Pref value used in tests, should reflect the 'checked' attribute.
         * Create a new pref for each test() to prevent order (state)
         * dependencies between tests.
         * @type {chrome.settingsPrivate.PrefObject}
         */
        var pref = {
          key: 'test',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true
        };
        PolymerTest.clearBody();
        testElement = document.createElement('settings-toggle-button');
        testElement.set('pref', pref);
        document.body.appendChild(testElement);
      });

      test('value changes on tap', function() {
        assertTrue(testElement.checked);
        assertTrue(testElement.pref.value);

        MockInteractions.tap(testElement.$.control);
        assertFalse(testElement.checked);
        assertFalse(testElement.pref.value);

        MockInteractions.tap(testElement.$.control);
        assertTrue(testElement.checked);
        assertTrue(testElement.pref.value);
      });

      test('fires a change event', function(done) {
        testElement.addEventListener('change', function() {
          assertFalse(testElement.checked);
          done();
        });
        assertTrue(testElement.checked);
        MockInteractions.tap(testElement.$.control);
      });

      test('does not change when disabled', function() {
        testElement.checked = false;
        testElement.setAttribute('disabled', '');
        assertTrue(testElement.disabled);
        assertTrue(testElement.$.control.disabled);

        MockInteractions.tap(testElement.$.control);
        assertFalse(testElement.checked);
        assertFalse(testElement.$.control.checked);
      });

      test('inverted', function() {
        testElement.inverted = true;
        testElement.set('pref', {
          key: 'test',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true
        });

        assertTrue(testElement.pref.value);
        assertFalse(testElement.checked);

        MockInteractions.tap(testElement.$.control);
        assertFalse(testElement.pref.value);
        assertTrue(testElement.checked);

        MockInteractions.tap(testElement.$.control);
        assertTrue(testElement.pref.value);
        assertFalse(testElement.checked);
      });

      test('numerical pref', function() {
        var prefNum = {
          key: 'test',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 1
        };

        testElement.set('pref', prefNum);
        assertTrue(testElement.checked);

        MockInteractions.tap(testElement.$.control);
        assertFalse(testElement.checked);
        assertEquals(0, prefNum.value);

        MockInteractions.tap(testElement.$.control);
        assertTrue(testElement.checked);
        assertEquals(1, prefNum.value);
      });

      test('numerical pref with custom values', function() {
        var prefNum = {
          key: 'test',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 5
        };

        testElement._setNumericUncheckedValue(5);

        testElement.set('pref', prefNum);
        assertFalse(testElement.checked);

        MockInteractions.tap(testElement.$.control);
        assertTrue(testElement.checked);
        assertEquals(1, prefNum.value);

        MockInteractions.tap(testElement.$.control);
        assertFalse(testElement.checked);
        assertEquals(5, prefNum.value);
      });

      test('numerical pref with unknown inital value', function() {
        prefNum = {
          key: 'test',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 3
        };

        testElement._setNumericUncheckedValue(5);

        testElement.set('pref', prefNum);

        // Unknown value should still count as checked.
        assertTrue(testElement.checked);

        // The control should not clobber an existing unknown value.
        assertEquals(3, prefNum.value);

        // Unchecking should still send the unchecked value to prefs.
        MockInteractions.tap(testElement.$.control);
        assertFalse(testElement.checked);
        assertEquals(5, prefNum.value);

        // Checking should still send the normal checked value to prefs.
        MockInteractions.tap(testElement.$.control);
        assertTrue(testElement.checked);
        assertEquals(1, prefNum.value);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
