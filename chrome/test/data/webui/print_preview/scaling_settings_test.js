// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('scaling_settings_test', function() {
  /** @enum {string} */
  const TestNames = {
    InputNotDisabledOnValidityChange: 'input not disabled on validity change',
  };

  const suiteName = 'ScalingSettingsTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewScalingSettingsElement} */
    let scalingSection = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      scalingSection = document.createElement('print-preview-scaling-settings');
      scalingSection.settings = {
        scaling: {
          value: '100',
          unavailableValue: '100',
          valid: true,
          available: true,
          key: 'scaleFactor',
        },
        customScaling: {
          value: false,
          unavailableValue: false,
          valid: true,
          available: true,
          key: 'customScaling',
        },
        fitToPage: {
          value: false,
          unavailableValue: false,
          valid: true,
          available: false,
          key: 'isFitToPageEnabled',
        },
      };
      scalingSection.disabled = false;
      document.body.appendChild(scalingSection);
    });

    /**
     * Sets up the scaling section to use the custom input with the input string
     * given by |inputString|.
     * @param {string} inputString
     * @param {number} pageCount
     * @return {!Promise} Promise that resolves when the input-change event
     *     has fired.
     */
    function setupInput(inputString) {
      const numberSection =
          scalingSection.$$('print-preview-number-settings-section');
      const input = numberSection.getInput();
      const scalingSelect = scalingSection.$$('select');
      const isCustomSelected = scalingSelect.value ===
          scalingSection.scalingValueEnum_.CUSTOM.toString();
      const readyForInput = isCustomSelected ?
          Promise.resolve() :
          test_util.eventToPromise('process-select-change', scalingSection);

      // Select custom
      if (!isCustomSelected) {
        scalingSelect.value =
            scalingSection.scalingValueEnum_.CUSTOM.toString();
        scalingSelect.dispatchEvent(new CustomEvent('change'));
      }
      return readyForInput.then(() => {
        // Set input string
        input.value = inputString;
        input.dispatchEvent(
            new CustomEvent('input', {composed: true, bubbles: true}));

        // Validate results
        return test_util.eventToPromise('input-change', numberSection);
      });
    }

    /**
     * @param {string} expectedScaling The expected scaling value.
     * @param {boolean} invalid Whether the scaling setting should be invalid.
     */
    function validateState(expectedScaling, invalid) {
      assertEquals(expectedScaling, scalingSection.getSettingValue('scaling'));
      assertEquals(!invalid, scalingSection.getSetting('scaling').valid);
      assertEquals(
          invalid,
          scalingSection.$$('print-preview-number-settings-section')
              .getInput()
              .invalid);
    }

    // Verifies that the input is never disabled when the validity of the
    // setting changes.
    test(assert(TestNames.InputNotDisabledOnValidityChange), function() {
      // In the real UI, the print preview app listens for this event from this
      // section and others and sets disabled to true if any change from true to
      // false is detected. Imitate this here. Since we are only interacting
      // with the scaling input, at no point should the input be disabled, as it
      // will lose focus.
      scalingSection.addEventListener('setting-valid-changed', function(e) {
        const numberSection =
            scalingSection.$$('print-preview-number-settings-section');
        assertFalse(numberSection.getInput().disabled);
      });

      // Set a valid input
      return setupInput('90')
          .then(function() {
            validateState('90', false);
            // Set invalid input
            return setupInput('9');
          })
          .then(function() {
            validateState('90', true);
            // Restore valid input
            return setupInput('90');
          })
          .then(function() {
            validateState('90', false);
            // Invalid input again
            return setupInput('9');
          })
          .then(function() {
            validateState('90', true);
            // Clear input
            return setupInput('');
          })
          .then(function() {
            validateState('90', false);
            // Set valid input
            return setupInput('50');
          })
          .then(function() {
            validateState('50', false);
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
