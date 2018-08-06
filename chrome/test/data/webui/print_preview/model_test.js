// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('model_test', function() {
  /** @enum {string} */
  const TestNames = {
    SetStickySettings: 'set sticky settings',
    SetPolicySettings: 'set policy settings',
    GetPrintTicket: 'get print ticket',
    GetCloudPrintTicket: 'get cloud print ticket',
  };

  const suiteName = 'ModelTest';
  suite(suiteName, function() {
    let model = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      model = document.createElement('print-preview-model');
      document.body.appendChild(model);
    });

    /**
     * Tests state restoration with all boolean settings set to true, scaling =
     * 90, dpi = 100, custom square paper, and custom margins.
     */
    test(assert(TestNames.SetStickySettings), function() {
      // Default state of the model.
      const stickySettingsDefault = {
        version: 2,
        recentDestinations: [],
        dpi: {},
        mediaSize: {width_microns: 215900, height_microns: 279400},
        marginsType: 0, /* default */
        scaling: '100',
        isHeaderFooterEnabled: true,
        isCssBackgroundEnabled: false,
        isFitToPageEnabled: false,
        isCollateEnabled: true,
        isDuplexEnabled: true,
        isLandscapeEnabled: false,
        isColorEnabled: true,
        vendorOptions: {},
      };

      // Non-default state
      const stickySettingsChange = {
        version: 2,
        recentDestinations: [],
        dpi: {horizontal_dpi: 1000, vertical_dpi: 500},
        mediaSize: {width_microns: 43180, height_microns: 21590},
        marginsType: 2, /* none */
        scaling: '85',
        isHeaderFooterEnabled: false,
        isCssBackgroundEnabled: true,
        isFitToPageEnabled: true,
        isCollateEnabled: false,
        isDuplexEnabled: false,
        isLandscapeEnabled: true,
        isColorEnabled: false,
        vendorOptions: {
          paperType: 1,
          printArea: 6,
        },
      };

      /**
       * @param {string} setting The name of the setting to check.
       * @param {string} field The name of the field in the serialized state
       *     corresponding to the setting.
       * @return {!Promise} Promise that resolves when the setting has been set,
       *     the saved string has been validated, and the setting has been
       *     reset to its default value.
       */
      const testStickySetting = function(setting, field) {
        let promise = test_util.eventToPromise('save-sticky-settings', model);
        model.set(`settings.${setting}.value`, stickySettingsChange[field]);
        return promise.then(
            /**
             * @param {!CustomEvent} e Event containing the serialized settings
             * @return {!Promise} Promise that resolves when setting is reset.
             */
            function(e) {
              let settings = JSON.parse(e.detail);
              Object.keys(stickySettingsDefault).forEach(settingName => {
                let toCompare = settingName == field ? stickySettingsChange :
                                                       stickySettingsDefault;
                assertDeepEquals(toCompare[settingName], settings[settingName]);
              });
              let restorePromise =
                  test_util.eventToPromise('save-sticky-settings', model);
              model.set(
                  `settings.${setting}.value`, stickySettingsDefault[field]);
              return restorePromise;
            });
      };

      model.applyStickySettings();
      return testStickySetting('collate', 'isCollateEnabled')
          .then(() => testStickySetting('color', 'isColorEnabled'))
          .then(
              () =>
                  testStickySetting('cssBackground', 'isCssBackgroundEnabled'))
          .then(() => testStickySetting('dpi', 'dpi'))
          .then(() => testStickySetting('duplex', 'isDuplexEnabled'))
          .then(() => testStickySetting('fitToPage', 'isFitToPageEnabled'))
          .then(
              () => testStickySetting('headerFooter', 'isHeaderFooterEnabled'))
          .then(() => testStickySetting('layout', 'isLandscapeEnabled'))
          .then(() => testStickySetting('margins', 'marginsType'))
          .then(() => testStickySetting('mediaSize', 'mediaSize'))
          .then(() => testStickySetting('scaling', 'scaling'))
          .then(() => testStickySetting('fitToPage', 'isFitToPageEnabled'))
          .then(() => testStickySetting('vendorItems', 'vendorOptions'));
    });

    /**
     * Tests that setSetting() won't change the value if there is already a
     * policy for that setting.
     */
    test(assert(TestNames.SetPolicySettings), function() {
      model.setSetting('headerFooter', false);
      assertFalse(model.settings.headerFooter.value);

      model.setPolicySettings(undefined);
      // undefined is treated as 'no policy', and doesn't affect the value.
      assertFalse(model.settings.headerFooter.value);

      model.setPolicySettings(true);
      assertTrue(model.settings.headerFooter.value);

      model.setSetting('headerFooter', false);
      // The value didn't change after setSetting(), because the policy takes
      // priority.
      assertTrue(model.settings.headerFooter.value);
    });

    function toggleSettings() {
      // Some non default setting values to change to.
      const settingsChange = {
        pages: [2],
        copies: '2',
        collate: false,
        layout: true,
        color: false,
        mediaSize: {
          width_microns: 240000,
          height_microns: 200000,
        },
        margins: print_preview.ticket_items.MarginsTypeValue.CUSTOM,
        customMargins: {
          marginTop: 100,
          marginRight: 200,
          marginBottom: 300,
          marginLeft: 400,
        },
        dpi: {
          horizontal_dpi: 100,
          vertical_dpi: 100,
        },
        fitToPage: true,
        scaling: '90',
        duplex: false,
        cssBackground: true,
        selectionOnly: true,
        headerFooter: false,
        rasterize: true,
        vendorItems: {
          paperType: 1,
          printArea: 6,
        },
        ranges: [{from: 2, to: 2}],
      };

      // Update settings
      Object.keys(settingsChange).forEach(setting => {
        model.set(`settings.${setting}.value`, settingsChange[setting]);
      });
    }

    function initializeModel() {
      model.documentInfo = new print_preview.DocumentInfo();
      model.documentInfo.init(true, 'title', true);
      model.documentInfo.updatePageCount(3);
      // Update pages accordingly.
      model.set('settings.pages.value', [1, 2, 3]);

      // Initialize some settings that don't have defaults to the destination
      // defaults.
      model.set('settings.dpi.value', {horizontal_dpi: 200, vertical_dpi: 200});
      model.set('settings.vendorItems.value', {paperType: 0, printArea: 4});

      // Set rasterize available so that it can be tested.
      model.set('settings.rasterize.available', true);
    }

    /**
     * Tests that toggling each setting results in the expected change to the
     * print ticket.
     */
    test(assert(TestNames.GetPrintTicket), function() {
      const testDestination = new print_preview.Destination(
          'FooDevice', print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL, 'FooName', true /* isRecent */,
          print_preview.DestinationConnectionStatus.ONLINE);
      testDestination.capabilities =
          print_preview_test_utils.getCddTemplateWithAdvancedSettings(2)
              .capabilities;

      initializeModel();
      const defaultTicket =
          model.createPrintTicket(testDestination, false, false);
      const expectedDefaultTicket = JSON.stringify({
        mediaSize: {width_microns: 215900, height_microns: 279400},
        pageCount: 3,
        landscape: false,
        color: testDestination.getNativeColorModel(true),
        headerFooterEnabled: false,  // Only used in print preview
        marginsType: print_preview.ticket_items.MarginsTypeValue.DEFAULT,
        duplex: print_preview_new.DuplexMode.LONG_EDGE,
        copies: 1,
        collate: true,
        shouldPrintBackgrounds: false,
        shouldPrintSelectionOnly: false,
        previewModifiable: true,
        printToPDF: false,
        printWithCloudPrint: false,
        printWithPrivet: false,
        printWithExtension: false,
        rasterizePDF: false,
        scaleFactor: 100,
        pagesPerSheet: 1,
        dpiHorizontal: 200,
        dpiVertical: 200,
        deviceName: 'FooDevice',
        fitToPageEnabled: false,
        pageWidth: 612,
        pageHeight: 792,
        showSystemDialog: false,
      });
      expectEquals(expectedDefaultTicket, defaultTicket);

      // Toggle all the values and create a new print ticket.
      toggleSettings();
      const newTicket = model.createPrintTicket(testDestination, false, false);
      const expectedNewTicket = JSON.stringify({
        mediaSize: {width_microns: 240000, height_microns: 200000},
        pageCount: 1,
        landscape: true,
        color: testDestination.getNativeColorModel(false),
        headerFooterEnabled: false,
        marginsType: print_preview.ticket_items.MarginsTypeValue.CUSTOM,
        duplex: print_preview_new.DuplexMode.SIMPLEX,
        copies: 2,
        collate: false,
        shouldPrintBackgrounds: true,
        shouldPrintSelectionOnly: false,  // Only for Print Preview.
        previewModifiable: true,
        printToPDF: false,
        printWithCloudPrint: false,
        printWithPrivet: false,
        printWithExtension: false,
        rasterizePDF: true,
        scaleFactor: 90,
        pagesPerSheet: 1,
        dpiHorizontal: 100,
        dpiVertical: 100,
        deviceName: 'FooDevice',
        fitToPageEnabled: true,
        pageWidth: 612,
        pageHeight: 792,
        showSystemDialog: false,
        marginsCustom: {
          marginTop: 100,
          marginRight: 200,
          marginBottom: 300,
          marginLeft: 400,
        },
      });
      expectEquals(expectedNewTicket, newTicket);
    });

    /**
     * Tests that toggling each setting results in the expected change to the
     * cloud job print ticket.
     */
    test(assert(TestNames.GetCloudPrintTicket), function() {
      initializeModel();

      // Create a test cloud destination.
      const testDestination = new print_preview.Destination(
          'FooCloudDevice', print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, 'FooCloudName',
          true /* isRecent */,
          print_preview.DestinationConnectionStatus.ONLINE);
      testDestination.capabilities =
          print_preview_test_utils.getCddTemplateWithAdvancedSettings(2)
              .capabilities;

      const defaultTicket = model.createCloudJobTicket(testDestination);
      const expectedDefaultTicket = JSON.stringify({
        version: '1.0',
        print: {
          collate: {collate: true},
          color: {
            type: testDestination.getSelectedColorOption(true).type,
          },
          copies: {copies: 1},
          duplex: {type: 'LONG_EDGE'},
          media_size: {
            width_microns: 215900,
            height_microns: 279400,
          },
          page_orientation: {type: 'PORTRAIT'},
          dpi: {
            horizontal_dpi: 200,
            vertical_dpi: 200,
          },
          vendor_ticket_item: [
            {id: 'paperType', value: 0},
            {id: 'printArea', value: 4},
          ],
        },
      });
      expectEquals(expectedDefaultTicket, defaultTicket);

      // Toggle all the values and create a new cloud job ticket.
      toggleSettings();
      const newTicket = model.createCloudJobTicket(testDestination);
      const expectedNewTicket = JSON.stringify({
        version: '1.0',
        print: {
          collate: {collate: false},
          color: {
            type: testDestination.getSelectedColorOption(false).type,
          },
          copies: {copies: 2},
          duplex: {type: 'NO_DUPLEX'},
          media_size: {
            width_microns: 240000,
            height_microns: 200000,
          },
          page_orientation: {type: 'LANDSCAPE'},
          dpi: {
            horizontal_dpi: 100,
            vertical_dpi: 100,
          },
          vendor_ticket_item: [
            {id: 'paperType', value: 1},
            {id: 'printArea', value: 6},
          ],
        },
      });
      expectEquals(expectedNewTicket, newTicket);
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
