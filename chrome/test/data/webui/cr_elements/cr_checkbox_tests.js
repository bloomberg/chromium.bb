// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-checkbox. */
cr.define('cr_checkbox', function() {
  function registerTests() {
    suite('CrCheckbox', function() {
      /**
       * Checkbox created before each test.
       * @type {CrCheckbox}
       */
      var checkbox;

      // Import cr_checkbox.html before running suite.
      suiteSetup(function(done) {
        PolymerTest.importHref(
            'chrome://resources/cr_elements/v1_0/cr_checkbox/cr_checkbox.html',
            done);
      });

      // Initialize a checked cr-checkbox before each test.
      setup(function(done) {
        PolymerTest.clearBody();
        checkbox = document.createElement('cr-checkbox');
        checkbox.setAttribute('checked', '');
        document.body.appendChild(checkbox);

        // Allow for the checkbox to be created and attached.
        setTimeout(done);
      });

      test('can toggle', function() {
        assertTrue(checkbox.checked);

        checkbox.toggle();
        assertFalse(checkbox.checked);
        assertFalse(checkbox.hasAttribute('checked'));

        checkbox.toggle();
        assertTrue(checkbox.checked);
        assertTrue(checkbox.hasAttribute('checked'));
      });

      test('responds to checked attribute', function() {
        assertTrue(checkbox.checked);

        checkbox.removeAttribute('checked');
        assertFalse(checkbox.checked);

        checkbox.setAttribute('checked', '');
        assertTrue(checkbox.checked);
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
