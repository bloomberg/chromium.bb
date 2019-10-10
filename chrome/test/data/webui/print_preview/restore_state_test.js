// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('restore_state_test', function() {
  /** @enum {string} */
  const TestNames = {
    RestoreTrueValues: 'restore true values',
    RestoreFalseValues: 'restore false values',
    SaveValues: 'save values',
  };

  const suiteName = 'RestoreStateTest';
  suite(suiteName, function() {
    let page = null;
    let nativeLayer = null;

    const initialSettings =
        print_preview_test_utils.getDefaultInitialSettings();

    /** @override */
    setup(function() {
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      PolymerTest.clearBody();
    });

    /**
     * @param {!print_preview.SerializedSettings} stickySettings Settings
     *     to verify.
     */
    function verifyStickySettingsApplied(stickySettings) {
      assertEquals(
          stickySettings.dpi.horizontal_dpi,
          page.settings.dpi.value.horizontal_dpi);
      assertEquals(
          stickySettings.dpi.vertical_dpi,
          page.settings.dpi.value.vertical_dpi);
      assertEquals(
          stickySettings.mediaSize.name, page.settings.mediaSize.value.name);
      assertEquals(
          stickySettings.mediaSize.height_microns,
          page.settings.mediaSize.value.height_microns);
      assertEquals(
          stickySettings.mediaSize.width_microns,
          page.settings.mediaSize.value.width_microns);
      assertEquals(
          stickySettings.vendorOptions.paperType,
          page.settings.vendorItems.value.paperType);
      assertEquals(
          stickySettings.vendorOptions.printArea,
          page.settings.vendorItems.value.printArea);

      [['margins', 'marginsType'],
       ['color', 'isColorEnabled'],
       ['headerFooter', 'isHeaderFooterEnabled'],
       ['layout', 'isLandscapeEnabled'],
       ['collate', 'isCollateEnabled'],
       ['cssBackground', 'isCssBackgroundEnabled'],
       ['scaling', 'scaling'],
       ['scalingType', 'scalingType'],
       ['scalingTypePdf', 'scalingTypePdf'],
      ].forEach(keys => {
        assertEquals(stickySettings[keys[1]], page.settings[keys[0]].value);
      });
    }

    /**
     * @param {!print_preview.SerializedSettings} stickySettings
     * @return {!Promise} Promise that resolves when initialization is done and
     *     settings have been verified.
     */
    function testInitializeWithStickySettings(stickySettings) {
      initialSettings.serializedAppStateStr = JSON.stringify(stickySettings);

      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getCddTemplateWithAdvancedSettings(
              2, initialSettings.printerName));
      const pluginProxy = new print_preview.PDFPluginStub();
      print_preview.PluginProxy.setInstance(pluginProxy);

      page = document.createElement('print-preview-app');
      document.body.appendChild(page);
      const previewArea = page.$.previewArea;

      return nativeLayer.whenCalled('getInitialSettings')
          .then(function() {
            return nativeLayer.whenCalled('getPrinterCapabilities');
          })
          .then(function() {
            verifyStickySettingsApplied(stickySettings);
          });
    }

    /**
     * Tests state restoration with all boolean settings set to true, scaling =
     * 90, dpi = 100, custom square paper, and custom margins.
     */
    test(assert(TestNames.RestoreTrueValues), function() {
      const stickySettings = {
        version: 2,
        recentDestinations: [],
        dpi: {horizontal_dpi: 100, vertical_dpi: 100},
        mediaSize: {
          name: 'CUSTOM_SQUARE',
          width_microns: 215900,
          height_microns: 215900,
          custom_display_name: 'CUSTOM_SQUARE'
        },
        customMargins: {top: 74, right: 74, bottom: 74, left: 74},
        vendorOptions: {
          paperType: 1,
          printArea: 6,
        },
        marginsType: 3, /* custom */
        scaling: '90',
        scalingType: print_preview.ScalingType.CUSTOM,
        scalingTypePdf: print_preview.ScalingType.FIT_TO_PAGE,
        isHeaderFooterEnabled: true,
        isCssBackgroundEnabled: true,
        isCollateEnabled: true,
        isDuplexEnabled: true,
        isDuplexShortEdge: true,
        isLandscapeEnabled: true,
        isColorEnabled: true,
      };
      if (cr.isChromeOS) {
        stickySettings.pin = true;
        stickySettings.pinValue = '0000';
      }
      return testInitializeWithStickySettings(stickySettings);
    });

    /**
     * Tests state restoration with all boolean settings set to false, scaling =
     * 120, dpi = 200, letter paper and default margins.
     */
    test(assert(TestNames.RestoreFalseValues), function() {
      const stickySettings = {
        version: 2,
        recentDestinations: [],
        dpi: {horizontal_dpi: 200, vertical_dpi: 200},
        mediaSize: {
          name: 'NA_LETTER',
          width_microns: 215900,
          height_microns: 279400,
          is_default: true,
          custom_display_name: 'Letter'
        },
        customMargins: {},
        vendorOptions: {
          paperType: 0,
          printArea: 4,
        },
        marginsType: 0, /* default */
        scaling: '120',
        scalingType: print_preview.ScalingType.DEFAULT,
        scalingTypePdf: print_preview.ScalingType.DEFAULT,
        isHeaderFooterEnabled: false,
        isCssBackgroundEnabled: false,
        isCollateEnabled: false,
        isDuplexEnabled: false,
        isDuplexShortEdge: false,
        isLandscapeEnabled: false,
        isColorEnabled: false,
      };
      if (cr.isChromeOS) {
        stickySettings.pin = false;
        stickySettings.pinValue = '';
      }
      return testInitializeWithStickySettings(stickySettings);
    });

    /**
     * Tests that setting the settings values results in the correct serialized
     * values being sent to the native layer.
     */
    test(assert(TestNames.SaveValues), function() {
      /**
       * Array of section names, setting names, keys for serialized state, and
       * values for testing.
       * @type {Array<{section: string,
       *               settingName: string,
       *               key: string,
       *               value: *}>}
       */
      const testData = [
        {
          section: 'print-preview-copies-settings',
          settingName: 'collate',
          key: 'isCollateEnabled',
          value: true,
        },
        {
          section: 'print-preview-layout-settings',
          settingName: 'layout',
          key: 'isLandscapeEnabled',
          value: true,
        },
        {
          section: 'print-preview-color-settings',
          settingName: 'color',
          key: 'isColorEnabled',
          value: false,
        },
        {
          section: 'print-preview-media-size-settings',
          settingName: 'mediaSize',
          key: 'mediaSize',
          value: {
            name: 'CUSTOM_SQUARE',
            width_microns: 215900,
            height_microns: 215900,
            custom_display_name: 'CUSTOM_SQUARE',
          },
        },
        {
          section: 'print-preview-margins-settings',
          settingName: 'margins',
          key: 'marginsType',
          value: print_preview.MarginsType.MINIMUM,
        },
        {
          section: 'print-preview-dpi-settings',
          settingName: 'dpi',
          key: 'dpi',
          value: {horizontal_dpi: 100, vertical_dpi: 100},
        },
        {
          section: 'print-preview-scaling-settings',
          settingName: 'scalingType',
          key: 'scalingType',
          value: print_preview.ScalingType.CUSTOM,
        },
        {
          section: 'print-preview-scaling-settings',
          settingName: 'scalingTypePdf',
          key: 'scalingTypePdf',
          value: print_preview.ScalingType.CUSTOM,
        },
        {
          section: 'print-preview-scaling-settings',
          settingName: 'scaling',
          key: 'scaling',
          value: '85',
        },
        {
          section: 'print-preview-duplex-settings',
          settingName: 'duplex',
          key: 'isDuplexEnabled',
          value: false,
        },
        {
          section: 'print-preview-duplex-settings',
          settingName: 'duplexShortEdge',
          key: 'isDuplexShortEdge',
          value: true,
        },
        {
          section: 'print-preview-other-options-settings',
          settingName: 'headerFooter',
          key: 'isHeaderFooterEnabled',
          value: false,
        },
        {
          section: 'print-preview-other-options-settings',
          settingName: 'cssBackground',
          key: 'isCssBackgroundEnabled',
          value: true,
        },
        {
          section: 'print-preview-advanced-options-settings',
          settingName: 'vendorItems',
          key: 'vendorOptions',
          value: {
            paperType: 1,
            printArea: 6,
          },
        }
      ];
      if (cr.isChromeOS) {
        testData.push(
            {
              section: 'print-preview-pin-settings',
              settingName: 'pin',
              key: 'isPinEnabled',
              value: true,
            },
            {
              section: 'print-preview-pin-settings',
              settingName: 'pinValue',
              key: 'pinValue',
              value: '0000',
            });
      }

      // Setup
      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getCddTemplate(initialSettings.printerName));

      page = document.createElement('print-preview-app');
      document.body.appendChild(page);
      const previewArea = page.$$('print-preview-preview-area');
      previewArea.plugin_ = new print_preview.PDFPluginStub(
          previewArea.onPluginLoad_.bind(previewArea));

      return nativeLayer.whenCalled('getInitialSettings')
          .then(function() {
            return nativeLayer.whenCalled('getPrinterCapabilities');
          })
          .then(function() {
            // Set all the settings sections.
            testData.forEach((testValue, index) => {
              if (index == testData.length - 1) {
                nativeLayer.resetResolver('saveAppState');
              }
              // Since advanced options settings doesn't set this setting in
              // production, just use the model instead of creating the dialog.
              const element = testValue.settingName === 'vendorItems' ?
                  print_preview.Model.getInstance() :
                  page.$$('print-preview-sidebar').$$(testValue.section);
              element.setSetting(testValue.settingName, testValue.value);
            });
            // Wait on only the last call to saveAppState, which should
            // contain all the update settings values.
            return nativeLayer.whenCalled('saveAppState');
          })
          .then(function(serializedSettingsStr) {
            const serializedSettings = JSON.parse(serializedSettingsStr);
            // Validate serialized state.
            testData.forEach(testValue => {
              expectEquals(
                  JSON.stringify(testValue.value),
                  JSON.stringify(serializedSettings[testValue.key]));
            });
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
