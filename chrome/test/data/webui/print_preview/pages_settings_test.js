// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('pages_settings_test', function() {
  /** @enum {string} */
  const TestNames = {
    ValidPageRanges: 'valid page ranges',
    InvalidPageRanges: 'invalid page ranges',
    NupChangesPages: 'nup changes pages',
  };

  const suiteName = 'PagesSettingsTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewPagesSettingsElement} */
    let pagesSection = null;

    /** @type {?print_preview.DocumentInfo} */
    let documentInfo = null;

    /** @override */
    setup(function() {
      documentInfo = new print_preview.DocumentInfo();
      documentInfo.init(true, 'title', false);

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
      pagesSection.documentInfo = documentInfo;
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
      documentInfo.updatePageCount(pageCount);
      pagesSection.notifyPath('documentInfo.pageCount');
      Polymer.dom.flush();

      const input = pagesSection.$.pageSettingsCustomInput.inputElement;
      const readyForInput = pagesSection.$$('#custom-radio-button').checked ?
          Promise.resolve() :
          test_util.eventToPromise('focus', input);

      // Select custom
      pagesSection.$$('#custom-radio-button').click();
      return readyForInput.then(() => {
        // Set input string
        input.value = inputString;
        input.dispatchEvent(
            new CustomEvent('input', {composed: true, bubbles: true}));

        // Validate results
        return test_util.eventToPromise('input-change', pagesSection);
      });
    }

    /** @param {!Array<number>} expectedPages The expected pages value. */
    function validateState(expectedPages) {
      const pagesValue = pagesSection.getSettingValue('pages');
      assertEquals(expectedPages.length, pagesValue.length);
      expectedPages.forEach((page, index) => {
        assertEquals(page, pagesValue[index]);
      });
      assertTrue(pagesSection.$$('cr-input').errorMessage.length === 0);
    }

    // Tests that the page ranges set are valid for different user inputs.
    test(assert(TestNames.ValidPageRanges), function() {
      const oneToHundred = Array.from({length: 100}, (x, i) => i + 1);
      const tenToHundred = Array.from({length: 91}, (x, i) => i + 10);

      return setupInput('1, 2, 3, 1, 56', 100)
          .then(function() {
            validateState([1, 2, 3, 56]);
            return setupInput('1-3, 6-9, 6-10', 100);
          })
          .then(function() {
            validateState([1, 2, 3, 6, 7, 8, 9, 10]);
            return setupInput('10-', 100);
          })
          .then(function() {
            validateState(tenToHundred);
            return setupInput('10-100', 100);
          })
          .then(function() {
            validateState(tenToHundred);
            return setupInput('-', 100);
          })
          .then(function() {
            validateState(oneToHundred);
            // https://crbug.com/806165
            return setupInput('1\u30012\u30013\u30011\u300156', 100);
          })
          .then(function() {
            validateState([1, 2, 3, 56]);
            return setupInput('1,2,3\u30011\u300156', 100);
          })
          .then(function() {
            validateState([1, 2, 3, 56]);
          });
    });

    // Tests that the correct error messages are shown for different user
    // inputs.
    test(assert(TestNames.InvalidPageRanges), function() {
      const limitError = 'Out of bounds page reference, limit is ';
      const syntaxError = 'Invalid page range, use e.g. 1-5, 8, 11-13';

      /** @param {string} expectedMessage The expected error message. */
      const validateErrorState = function(expectedMessage) {
        assertFalse(pagesSection.$$('cr-input').errorMessage.length === 0);
        assertEquals(expectedMessage, pagesSection.$$('cr-input').errorMessage);
      };

      return setupInput('10-100000', 100)
          .then(function() {
            validateErrorState(limitError + '100');
            return setupInput('1, 100000', 100);
          })
          .then(function() {
            validateErrorState(limitError + '100');
            return setupInput('1, 2, 0, 56', 100);
          })
          .then(function() {
            validateErrorState(syntaxError);
            return setupInput('-1, 1, 2,, 56', 100);
          })
          .then(function() {
            validateErrorState(syntaxError);
            return setupInput('1,2,56-40', 100);
          })
          .then(function() {
            validateErrorState(syntaxError);
            return setupInput('101-110', 100);
          })
          .then(function() {
            validateErrorState(limitError + '100');
            return setupInput('1\u30012\u30010\u300156', 100);
          })
          .then(function() {
            validateErrorState(syntaxError);
            return setupInput('-1,1,2\u3001\u300156', 100);
          })
          .then(function() {
            validateErrorState(syntaxError);
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
            validateState([1, 2, 3, 56]);
            pagesSection.setSetting('pagesPerSheet', 2);
            validateRanges(rangesValue);
            validateState([1, 2]);
            pagesSection.setSetting('pagesPerSheet', 4);
            validateRanges(rangesValue);
            validateState([1]);
            pagesSection.setSetting('pagesPerSheet', 1);
            return setupInput('1-3, 6-9, 6-10', 100);
          })
          .then(function() {
            const rangesValue =
                JSON.stringify(pagesSection.getSettingValue('ranges'));
            validateState([1, 2, 3, 6, 7, 8, 9, 10]);
            pagesSection.setSetting('pagesPerSheet', 2);
            validateRanges(rangesValue);
            validateState([1, 2, 3, 4]);
            pagesSection.setSetting('pagesPerSheet', 3);
            validateRanges(rangesValue);
            validateState([1, 2, 3]);
            return setupInput('1-3', 100);
          })
          .then(function() {
            const rangesValue =
                JSON.stringify(pagesSection.getSettingValue('ranges'));
            validateState([1]);
            pagesSection.setSetting('pagesPerSheet', 1);
            validateRanges(rangesValue);
            validateState([1, 2, 3]);
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
