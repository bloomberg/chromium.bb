// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('other_options_settings_test', function() {
  suite('OtherOptionsSettingsTest', function() {
    /** @type {?PrintPreviewOtherOptionsSettingsElement} */
    let otherOptionsSection = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      otherOptionsSection =
          document.createElement('print-preview-other-options-settings');
      otherOptionsSection.settings = {
        duplex: {
          value: true,
          unavailableValue: false,
          valid: true,
          available: true,
          setByPolicy: false,
          key: 'isDuplexEnabled',
        },
        cssBackground: {
          value: true,
          unavailableValue: false,
          valid: true,
          available: true,
          setByPolicy: false,
          key: 'isCssBackgroundEnabled',
        },
        selectionOnly: {
          value: true,
          unavailableValue: false,
          valid: true,
          available: true,
          setByPolicy: false,
          key: '',
        },
        headerFooter: {
          value: true,
          unavailableValue: false,
          valid: true,
          available: true,
          setByPolicy: false,
          key: 'isHeaderFooterEnabled',
        },
        rasterize: {
          value: true,
          unavailableValue: false,
          valid: true,
          available: true,
          setByPolicy: false,
          key: '',
        },
      };
      otherOptionsSection.disabled = false;
      document.body.appendChild(otherOptionsSection);
      Polymer.dom.flush();
    });

    /**
     * @param {!CrCheckboxElement} checkbox The checkbox to check
     * @return {boolean} Whether the checkbox's parent section is hidden.
     */
    function isSectionHidden(checkbox) {
      return checkbox.parentNode.parentNode.hidden;
    }

    // Verifies that the correct checkboxes are hidden when different settings
    // are not available.
    test('checkbox visibility', function() {
      ['headerFooter', 'duplex', 'cssBackground', 'rasterize', 'selectionOnly']
          .forEach(setting => {
            const checkbox = otherOptionsSection.$$(`#${setting}`);
            // Show, hide and reset.
            [true, false, true].forEach(value => {
              otherOptionsSection.set(`settings.${setting}.available`, value);
              // Element expected to be visible when available.
              assertEquals(!value, isSectionHidden(checkbox));
            });
          });
    });

    test('set with checkbox', async () => {
      const testOptionCheckbox = (settingName) => {
        const element = otherOptionsSection.$$('#' + settingName);
        const optionSetting = otherOptionsSection.getSetting(settingName);
        assertFalse(isSectionHidden(element));
        assertTrue(element.checked);
        assertTrue(optionSetting.value);
        element.checked = false;
        element.dispatchEvent(new CustomEvent('change'));
        return test_util
            .eventToPromise('update-checkbox-setting', otherOptionsSection)
            .then(function(event) {
              assertEquals(element.id, event.detail);
              assertFalse(optionSetting.value);
            });
      };

      await testOptionCheckbox('headerFooter');
      await testOptionCheckbox('duplex');
      await testOptionCheckbox('cssBackground');
      await testOptionCheckbox('rasterize');
      await testOptionCheckbox('selectionOnly');
    });

    test('update from setting', function() {
      ['headerFooter', 'duplex', 'cssBackground', 'rasterize', 'selectionOnly']
          .forEach(setting => {
            const checkbox = otherOptionsSection.$$(`#${setting}`);
            // Set true and then false.
            [true, false].forEach(value => {
              otherOptionsSection.setSetting(setting, value);
              // Element expected to be checked when setting is true.
              assertEquals(value, checkbox.checked);
            });
          });
    });

    // Tests that if settings are enforced by enterprise policy the checkbox
    // is disabled.
    test('disabled by policy', function() {
      const policyControlledSettings =
          cr.isChromeOS ? ['headerFooter', 'duplex'] : ['headerFooter'];
      policyControlledSettings.forEach(setting => {
        const checkbox = otherOptionsSection.$$(`#${setting}`);
        // Set true and then false.
        [true, false].forEach(value => {
          otherOptionsSection.set(`settings.${setting}.setByPolicy`, value);
          // Element expected to be disabled when policy is set.
          assertEquals(value, checkbox.disabled);
        });
      });
    });
  });
});
