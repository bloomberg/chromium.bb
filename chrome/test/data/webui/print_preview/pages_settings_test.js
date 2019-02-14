// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('pages_settings_test', function() {
  /** @enum {string} */
  const TestNames = {
    ValidPageRanges: 'valid page ranges',
    InvalidPageRanges: 'invalid page ranges',
    NupChangesPages: 'nup changes pages',
    ClearInput: 'clear input',
    TabOrder: 'tab order',
    ClickingCustomFocusesInput: 'clicking custom focuses input',
    InputNotDisabledOnValidityChange: 'input not disabled on validity change',
    IgnoreInputKeyEvents: 'ignore input key events',
    EnterOnInputTriggersPrint: 'enter on input triggers print',
  };

  const suiteName = 'PagesSettingsTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewPagesSettingsElement} */
    let pagesSection = null;

    /** @type {!Array<number>} */
    const oneToHundred = Array.from({length: 100}, (x, i) => i + 1);

    /** @type {string} */
    const limitError = 'Out of bounds page reference, limit is ';

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      pagesSection = document.createElement('print-preview-pages-settings');
      pagesSection.settings = {
        pages: {
          value: [1],
          unavailableValue: [],
          valid: true,
          available: true,
          key: '',
        },
        ranges: {
          value: [],
          unavailableValue: [],
          valid: true,
          available: true,
          key: '',
        },
        pagesPerSheet: {
          value: 1,
          unavailableValue: 1,
          valid: true,
          available: true,
          key: '',
        },
      };
      pagesSection.disabled = false;
      document.body.appendChild(pagesSection);
    });

    /**
     * Sets up the pages section to use the custom input with the input string
     * given by |inputString|, with the document page count set to |pageCount|
     * @param {string} inputString
     * @param {number} pageCount
     * @return {!Promise} Promise that resolves when the input-change event
     *     has fired.
     */
    function setupInput(inputString, pageCount) {
      // Set page count.
      pagesSection.pageCount = pageCount;
      Polymer.dom.flush();
      const input = pagesSection.$.pageSettingsCustomInput.inputElement;
      const pagesSelect = pagesSection.$$('select');
      const isCustomSelected =
          pagesSelect.value === pagesSection.pagesValueEnum_.CUSTOM.toString();
      const readyForInput = isCustomSelected ?
          Promise.resolve() :
          test_util.eventToPromise('process-select-change', pagesSection);

      // Select custom
      if (!isCustomSelected) {
        pagesSelect.value = pagesSection.pagesValueEnum_.CUSTOM.toString();
        pagesSelect.dispatchEvent(new CustomEvent('change'));
      }
      return readyForInput.then(() => {
        input.focus();
        // Set input string
        input.value = inputString;
        input.dispatchEvent(
            new CustomEvent('input', {composed: true, bubbles: true}));

        // Validate results
        return test_util.eventToPromise('input-change', pagesSection);
      });
    }

    /**
     * @param {!Array<number>} expectedPages The expected pages value.
     * @param {string} expectedError The expected error message.
     * @param {boolean} invalid Whether the pages setting should be invalid.
     */
    function validateState(expectedPages, expectedError, invalid) {
      const pagesValue = pagesSection.getSettingValue('pages');
      assertEquals(expectedPages.length, pagesValue.length);
      expectedPages.forEach((page, index) => {
        assertEquals(page, pagesValue[index]);
      });
      assertEquals(!invalid, pagesSection.getSetting('pages').valid);
      assertEquals(expectedError !== '', pagesSection.$$('cr-input').invalid);
      assertEquals(expectedError, pagesSection.$$('cr-input').errorMessage);
    }

    // Tests that the page ranges set are valid for different user inputs.
    test(assert(TestNames.ValidPageRanges), function() {
      const tenToHundred = Array.from({length: 91}, (x, i) => i + 10);

      return setupInput('1, 2, 3, 1, 56', 100)
          .then(function() {
            validateState([1, 2, 3, 56], '', false);
            return setupInput('1-3, 6-9, 6-10', 100);
          })
          .then(function() {
            validateState([1, 2, 3, 6, 7, 8, 9, 10], '', false);
            return setupInput('10-', 100);
          })
          .then(function() {
            validateState(tenToHundred, '', false);
            return setupInput('10-100', 100);
          })
          .then(function() {
            validateState(tenToHundred, '', false);
            return setupInput('-', 100);
          })
          .then(function() {
            validateState(oneToHundred, '', false);
            // https://crbug.com/806165
            return setupInput('1\u30012\u30013\u30011\u300156', 100);
          })
          .then(function() {
            validateState([1, 2, 3, 56], '', false);
            return setupInput('1,2,3\u30011\u300156', 100);
          })
          .then(function() {
            validateState([1, 2, 3, 56], '', false);
          });
    });

    // Tests that the correct error messages are shown for different user
    // inputs.
    test(assert(TestNames.InvalidPageRanges), function() {
      const syntaxError = 'Invalid page range, use e.g. 1-5, 8, 11-13';

      return setupInput('10-100000', 100)
          .then(function() {
            validateState(oneToHundred, limitError + '100', true);
            return setupInput('1, 100000', 100);
          })
          .then(function() {
            validateState(oneToHundred, limitError + '100', true);
            return setupInput('1, 2, 0, 56', 100);
          })
          .then(function() {
            validateState(oneToHundred, syntaxError, true);
            return setupInput('-1, 1, 2,, 56', 100);
          })
          .then(function() {
            validateState(oneToHundred, syntaxError, true);
            return setupInput('1,2,56-40', 100);
          })
          .then(function() {
            validateState(oneToHundred, syntaxError, true);
            return setupInput('101-110', 100);
          })
          .then(function() {
            validateState(oneToHundred, limitError + '100', true);
            return setupInput('1\u30012\u30010\u300156', 100);
          })
          .then(function() {
            validateState(oneToHundred, syntaxError, true);
            return setupInput('-1,1,2\u3001\u300156', 100);
          })
          .then(function() {
            validateState(oneToHundred, syntaxError, true);
            return setupInput('--', 100);
          })
          .then(function() {
            validateState(oneToHundred, syntaxError, true);
            return setupInput(' 1 1 ', 100);
          })
          .then(function() {
            validateState(oneToHundred, syntaxError, true);
          });
    });

    // Tests that the pages are set correctly for different values of pages per
    // sheet, and that ranges remain fixed (since they are used for generating
    // the print preview ticket).
    test(assert(TestNames.NupChangesPages), function() {
      /**
       * @param {string} rangesValue The desired stringified ranges setting
       *     value.
       */
      const validateRanges = function(rangesValue) {
        assertEquals(
            rangesValue,
            JSON.stringify(pagesSection.getSettingValue('ranges')));
      };
      return setupInput('1, 2, 3, 1, 56', 100)
          .then(function() {
            const rangesValue =
                JSON.stringify(pagesSection.getSettingValue('ranges'));
            validateState([1, 2, 3, 56], '', false);
            pagesSection.setSetting('pagesPerSheet', 2);
            validateRanges(rangesValue);
            validateState([1, 2], '', false);
            pagesSection.setSetting('pagesPerSheet', 4);
            validateRanges(rangesValue);
            validateState([1], '', false);
            pagesSection.setSetting('pagesPerSheet', 1);
            return setupInput('1-3, 6-9, 6-10', 100);
          })
          .then(function() {
            const rangesValue =
                JSON.stringify(pagesSection.getSettingValue('ranges'));
            validateState([1, 2, 3, 6, 7, 8, 9, 10], '', false);
            pagesSection.setSetting('pagesPerSheet', 2);
            validateRanges(rangesValue);
            validateState([1, 2, 3, 4], '', false);
            pagesSection.setSetting('pagesPerSheet', 3);
            validateRanges(rangesValue);
            validateState([1, 2, 3], '', false);
            return setupInput('1-3', 100);
          })
          .then(function() {
            const rangesValue =
                JSON.stringify(pagesSection.getSettingValue('ranges'));
            validateState([1], '', false);
            pagesSection.setSetting('pagesPerSheet', 1);
            validateRanges(rangesValue);
            validateState([1, 2, 3], '', false);
          });
    });

    // Tests that the clearing a valid input has no effect, clearing an invalid
    // input does not show an error message but does not reset the preview, and
    // changing focus from an empty input in either case fills in the dropdown
    // with the full page range.
    test(assert(TestNames.ClearInput), function() {
      const input = pagesSection.$.pageSettingsCustomInput.inputElement;
      const select = pagesSection.$$('select');
      const allValue = pagesSection.pagesValueEnum_.ALL.toString();
      const customValue = pagesSection.pagesValueEnum_.CUSTOM.toString();
      assertEquals(allValue, select.value);
      return setupInput('1-2', 3)
          .then(function() {
            assertEquals(customValue, select.value);
            validateState([1, 2], '', false);
            return setupInput('', 3);
          })
          .then(function() {
            assertEquals(customValue, select.value);
            validateState([1, 2], '', false);
            const whenBlurred = test_util.eventToPromise('blur', input);
            input.blur();
            return whenBlurred;
          })
          .then(function() {
            // Blurring a blank field sets the full page range.
            assertEquals(customValue, select.value);
            validateState([1, 2, 3], '', false);
            assertEquals('1-3', input.value);
            return setupInput('5', 3);
          })
          .then(function() {
            assertEquals(customValue, select.value);
            // Invalid input doesn't change the preview.
            validateState([1, 2, 3], limitError + '3', true);
            return setupInput('', 3);
          })
          .then(function() {
            assertEquals(customValue, select.value);
            validateState([1, 2, 3], '', false);
            const whenBlurred = test_util.eventToPromise('blur', input);
            input.blur();
            // Blurring an invalid value that has been cleared should reset the
            // value to all pages.
            return whenBlurred;
          })
          .then(function() {
            assertEquals(customValue, select.value);
            validateState([1, 2, 3], '', false);
            assertEquals('1-3', input.value);

            // Clear the input and then select "All" in the dropdown.
            input.focus();
            return setupInput('', 3);
          })
          .then(function() {
            select.focus();
            select.value = pagesSection.pagesValueEnum_.ALL.toString();
            select.dispatchEvent(new CustomEvent('change'));
            return test_util.eventToPromise(
                'process-select-change', pagesSection);
          })
          .then(function() {
            Polymer.dom.flush();
            assertEquals(allValue, select.value);
            validateState([1, 2, 3], '', false);
            // Reselect custom.
            select.value = pagesSection.pagesValueEnum_.CUSTOM.toString();
            select.dispatchEvent(new CustomEvent('change'));
            return test_util.eventToPromise('focus', input);
          })
          .then(function() {
            // Input has been cleared.
            assertEquals('', input.value);
            validateState([1, 2, 3], '', false);
          });
    });

    test(assert(TestNames.ClickingCustomFocusesInput), function() {
      const input = pagesSection.$.pageSettingsCustomInput.inputElement;
      const radioGroup = pagesSection.$$('cr-radio-group');
      assertEquals(pagesSection.pagesValueEnum_.ALL, radioGroup.selected);

      // Click the custom input and set a valid value.
      return setupInput('1-2', 3)
          .then(function() {
            // Blur the custom input.
            const whenCustomInputBlurred =
                test_util.eventToPromise('blur', input);
            input.blur();
            Polymer.dom.flush();
            return whenCustomInputBlurred;
          })
          .then(function() {
            const whenCustomInputFocused =
                test_util.eventToPromise('focus', input);
            // Clicking the custom radio button should re-focus the input.
            pagesSection.$.customRadioButton.click();
            return whenCustomInputFocused;
          });
    });

    // Verifies that the input is never disabled when the validity of the
    // setting changes.
    test(assert(TestNames.InputNotDisabledOnValidityChange), function() {
      // In the real UI, the print preview app listens for this event from this
      // section and others and sets disabled to true if any change from true to
      // false is detected. Imitate this here. Since we are only interacting
      // with the pages input, at no point should the input be disabled, as it
      // will lose focus.
      pagesSection.addEventListener('setting-valid-changed', function(e) {
        assertFalse(pagesSection.$.pageSettingsCustomInput.disabled);
        pagesSection.set('disabled', !e.detail);
        assertFalse(pagesSection.$.pageSettingsCustomInput.disabled);
      });

      const input = pagesSection.$.pageSettingsCustomInput.inputElement;

      // Set a valid input
      return setupInput('1', 3)
          .then(function() {
            validateState([1], '', false);
            // Set invalid input
            return setupInput('12', 3);
          })
          .then(function() {
            validateState([1], limitError + '3', true);
            // Restore valid input
            return setupInput('1', 3);
          })
          .then(function() {
            validateState([1], '', false);
            // Invalid input again
            return setupInput('8', 3);
          })
          .then(function() {
            validateState([1], limitError + '3', true);
            // Clear input
            return setupInput('', 3);
          })
          .then(function() {
            validateState([1], '', false);
            // Set valid input
            return setupInput('2', 3);
          })
          .then(function() {
            validateState([2], '', false);
          });
    });

    // Verifies that the enter key event is bubbled to the pages settings
    // element, so that it will be bubbled to the print preview app to trigger a
    // print.
    test(assert(TestNames.EnterOnInputTriggersPrint), function() {
      pagesSection.pageCount = 3;
      const input = pagesSection.$.pageSettingsCustomInput.inputElement;
      const whenPrintReceived =
          test_util.eventToPromise('keydown', pagesSection);

      // Setup an empty input by clicking on the custom radio button.
      const customValue = pagesSection.pagesValueEnum_.CUSTOM.toString();
      const pagesSelect = pagesSection.$$('select');
      pagesSelect.value = customValue;
      pagesSelect.dispatchEvent(new CustomEvent('change'));

      const inputFocused = test_util.eventToPromise('focus', input);
      return inputFocused
          .then(function() {
            assertEquals(customValue, pagesSelect.value);
            MockInteractions.keyEventOn(input, 'keydown', 13, [], 'Enter');
            return whenPrintReceived;
          })
          .then(function() {
            // Keep custom selected, but pages to print should still be all.
            assertEquals(customValue, pagesSelect.value);
            assertEquals(3, pagesSection.getSetting('pages').value.length);

            // Select a custom input of 1.
            return setupInput('1', 3);
          })
          // Re-select custom and print again.
          .then(function() {
            assertEquals(customValue, pagesSelect.value);
            const whenPrintReceived =
                test_util.eventToPromise('keydown', pagesSection);
            MockInteractions.keyEventOn(input, 'keydown', 13, [], 'Enter');
            return whenPrintReceived;
          })
          .then(function() {
            assertEquals(customValue, pagesSelect.value);
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
