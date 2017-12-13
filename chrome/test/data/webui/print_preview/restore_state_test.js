// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('restore_state_test', function() {
  /** @enum {string} */
  const TestNames = {
    RestoreTrueValues: 'restore true values',
    RestoreFalseValues: 'restore false values',
  };

  const suiteName = 'RestoreStateTest';
  suite(suiteName, function() {
    let page = null;
    let nativeLayer = null;

    const initialSettings = {
      isInKioskAutoPrintMode: false,
      isInAppKioskMode: false,
      thousandsDelimeter: ',',
      decimalDelimeter: '.',
      unitType: 1,
      previewModifiable: true,
      documentTitle: 'title',
      documentHasSelection: true,
      shouldPrintSelectionOnly: false,
      printerName: 'FooDevice',
      serializedAppStateStr: null,
      serializedDefaultDestinationSelectionRulesStr: null
    };

    /** @override */
    setup(function() {
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      PolymerTest.clearBody();
    });

    /**
     * @param {!print_preview_new.SerializedSettings} stickySettings Settings
     *     to verify.
     */
    function verifyStickySettingsApplied(stickySettings) {
      assertEquals(stickySettings.dpi.horizontal_dpi,
                   page.settings.dpi.value.horizontal_dpi);
      assertEquals(stickySettings.dpi.vertical_dpi,
                   page.settings.dpi.value.vertical_dpi);
      assertEquals(stickySettings.mediaSize.name,
                   page.settings.mediaSize.value.name);
      assertEquals(stickySettings.mediaSize.height_microns,
                   page.settings.mediaSize.value.height_microns);
      assertEquals(stickySettings.mediaSize.width_microns,
                   page.settings.mediaSize.value.width_microns);

      [['margins', 'marginsType'],
       ['color', 'isColorEnabled'], ['headerFooter', 'isHeaderFooterEnabled'],
       ['layout', 'isLandscapeEnabled'], ['collate', 'isCollateEnabled'],
       ['fitToPage', 'isFitToPageEnabled'],
       ['cssBackground', 'isCssBackgroundEnabled'], ['scaling', 'scaling'],
      ].forEach(keys => {
        assertEquals(stickySettings[keys[1]], page.settings[keys[0]].value);
      });
    }

    /**
     * @param {!print_preview_new.SerializedSettings} stickySettings
     * @return {!Promise} Promise that resolves when initialization is done and
     *     settings have been verified.
     */
    function testInitializeWithStickySettings(stickySettings) {
      initialSettings.serializedAppStateStr = JSON.stringify(stickySettings);

      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test.getCddTemplate(initialSettings.printerName));

      page = document.createElement('print-preview-app');
      document.body.appendChild(page);
      return nativeLayer.whenCalled('getInitialSettings').then(function() {
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
        mediaSize: {name: 'CUSTOM_SQUARE',
                    width_microns: 215900,
                    height_microns: 215900,
                    custom_display_name: 'CUSTOM_SQUARE'},
        customMargins: {top: 74, right: 74, bottom: 74, left: 74},
        vendorOptions: {},
        marginsType: 3,  /* custom */
        scaling: '90',
        isHeaderFooterEnabled: true,
        isCssBackgroundEnabled: true,
        isFitToPageEnabled: true,
        isCollateEnabled: true,
        isDuplexEnabled: true,
        isLandscapeEnabled: true,
        isColorEnabled: true,
      };
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
        mediaSize: {name: 'NA_LETTER',
                    width_microns: 215900,
                    height_microns: 279400,
                    is_default: true,
                    custom_display_name: 'Letter'},
        customMargins: {},
        vendorOptions: {},
        marginsType: 0,  /* default */
        scaling: '120',
        isHeaderFooterEnabled: false,
        isCssBackgroundEnabled: false,
        isFitToPageEnabled: false,
        isCollateEnabled: false,
        isDuplexEnabled: false,
        isLandscapeEnabled: false,
        isColorEnabled: false,
      };
      return testInitializeWithStickySettings(stickySettings);
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
