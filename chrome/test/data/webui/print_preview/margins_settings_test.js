// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('margins_settings_test', function() {
  suite('MarginsSettingsTest', function() {
    let marginsSection = null;

    let marginsTypeEnum = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      marginsSection = document.createElement('print-preview-margins-settings');
      document.body.appendChild(marginsSection);
      Polymer.dom.flush();
      marginsTypeEnum = print_preview.ticket_items.MarginsTypeValue;
      marginsSection.settings = {
        margins: {
          value: marginsTypeEnum.DEFAULT,
          unavailableValue: marginsTypeEnum.DEFAULT,
          valid: true,
          available: true,
          setByPolicy: false,
          key: 'marginsType',
        },
        pagesPerSheet: {
          value: 1,
          unavailableValue: 1,
          valid: true,
          available: true,
          setByPolicy: false,
          key: '',
        },
      };
      marginsSection.disabled = false;
    });

    // Tests that setting the setting updates the UI.
    test('set setting', async () => {
      const select = marginsSection.$$('select');
      assertEquals(marginsTypeEnum.DEFAULT.toString(), select.value);

      marginsSection.set('settings.margins.value', marginsTypeEnum.MINIMUM);
      await test_util.eventToPromise('process-select-change', marginsSection);
      assertEquals(marginsTypeEnum.MINIMUM.toString(), select.value);
    });

    // Tests that selecting a new option in the dropdown updates the setting.
    test('select option', async () => {
      // Verify that the selected option and names are as expected.
      const select = marginsSection.$$('select');
      assertEquals(marginsTypeEnum.DEFAULT.toString(), select.value);
      assertEquals(
          marginsTypeEnum.DEFAULT, marginsSection.getSettingValue('margins'));
      assertEquals(4, select.options.length);

      // Verify that selecting an new option in the dropdown sets the setting.
      await print_preview_test_utils.selectOption(
          marginsSection, marginsTypeEnum.MINIMUM.toString());
      assertEquals(
          marginsTypeEnum.MINIMUM, marginsSection.getSettingValue('margins'));
    });

    // This test verifies that changing pages per sheet to N > 1 disables the
    // margins dropdown.
    test('disabled by pages per sheet', function() {
      const select = marginsSection.$$('select');
      assertFalse(select.disabled);

      marginsSection.set('settings.pagesPerSheet.value', 2);
      assertTrue(select.disabled);

      marginsSection.set('settings.pagesPerSheet.value', 1);
      assertFalse(select.disabled);
    });
  });
});
