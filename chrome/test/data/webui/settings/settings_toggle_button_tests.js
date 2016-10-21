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

      /**
       * Pref value used in tests, should reflect the 'checked' attribute.
       * @type {SettingsCheckbox}
       */
      var pref = {
        key: 'test',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true
      };

      // Initialize a checked control before each test.
      setup(function() {
        PolymerTest.clearBody();
        testElement = document.createElement('settings-toggle-button');
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

      test('numerical pref', function() {
        var prefNum = {
          key: 'test',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 1
        };

        testElement.set('pref', prefNum);
        assertTrue(testElement.checked);

        testElement.removeAttribute('checked');
        assertFalse(testElement.checked);
        assertEquals(0, prefNum.value);

        testElement.setAttribute('checked', '');
        assertTrue(testElement.checked);
        assertEquals(1, prefNum.value);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
