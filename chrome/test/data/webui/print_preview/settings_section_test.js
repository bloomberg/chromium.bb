// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_sections_tests', function() {
  /** @enum {string} */
  const TestNames = {
    Copies: 'copies',
    Layout: 'layout',
    Color: 'color',
    MediaSize: 'media size',
    Margins: 'margins',
    Dpi: 'dpi',
    Scaling: 'scaling',
    Other: 'other',
    SetPages: 'set pages',
    SetCopies: 'set copies',
    SetLayout: 'set layout',
    SetColor: 'set color',
    SetMediaSize: 'set media size',
    SetDpi: 'set dpi',
    SetMargins: 'set margins',
    SetScaling: 'set scaling',
    SetOther: 'set other',
  };

  const suiteName = 'SettingsSectionsTests';
  suite(suiteName, function() {
    /** @type {?PrintPreviewAppElement} */
    let page = null;

    /** @override */
    setup(function() {
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

      const nativeLayer = new print_preview.NativeLayerStub();
      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getCddTemplate(initialSettings.printerName));
      nativeLayer.setPageCount(3);
      print_preview.NativeLayer.setInstance(nativeLayer);
      PolymerTest.clearBody();
      page = document.createElement('print-preview-app');
      const previewArea = page.$$('print-preview-preview-area');
      previewArea.plugin_ = new print_preview.PDFPluginStub(previewArea);
      document.body.appendChild(page);

      // Wait for initialization to complete.
      return Promise.all([
        nativeLayer.whenCalled('getInitialSettings'),
        nativeLayer.whenCalled('getPrinterCapabilities')
      ]).then(function() {
        Polymer.dom.flush();
      });
    });

    /**
     * @param {boolean} isPdf Whether the document should be a PDF.
     * @param {boolean} hasSelection Whether the document has a selection.
     */
    function initDocumentInfo(isPdf, hasSelection) {
      const info = new print_preview.DocumentInfo();
      info.init(!isPdf, 'title', hasSelection);
      if (isPdf)
        info.updateFitToPageScaling(98);
      info.updatePageCount(3);
      page.set('documentInfo_', info);
      Polymer.dom.flush();
    }

    function addSelection() {
      // Add a selection.
      let info = new print_preview.DocumentInfo();
      info.init(page.documentInfo_.isModifiable, 'title', true);
      page.set('documentInfo_', info);
      Polymer.dom.flush();
    }

    function setPdfDestination() {
      const saveAsPdfDestination = new print_preview.Destination(
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
          print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL,
          loadTimeData.getString('printToPDF'), false /*isRecent*/,
          print_preview.DestinationConnectionStatus.ONLINE);
      saveAsPdfDestination.capabilities =
          print_preview_test_utils.getCddTemplate(saveAsPdfDestination.id)
              .capabilities;
      page.set('destination_', saveAsPdfDestination);
    }

    test(assert(TestNames.Copies), function() {
      const copiesElement = page.$$('print-preview-copies-settings');
      expectEquals(false, copiesElement.hidden);

      // Remove copies capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.copies;

      // Copies section should now be hidden.
      page.set('destination_.capabilities', capabilities);
      expectEquals(true, copiesElement.hidden);
    });

    test(assert(TestNames.Layout), function() {
      const layoutElement = page.$$('print-preview-layout-settings');

      // Set up with HTML document. No selection.
      initDocumentInfo(false, false);
      expectEquals(false, layoutElement.hidden);

      // Remove layout capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.page_orientation;

      // Each of these settings should not show the capability.
      [null, {option: [{type: 'PORTRAIT', is_default: true}]},
       {option: [{type: 'LANDSCAPE', is_default: true}]},
      ].forEach(layoutCap => {
        capabilities =
            print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
        capabilities.printer.page_orientation = layoutCap;
        // Layout section should now be hidden.
        page.set('destination_.capabilities', capabilities);
        expectEquals(true, layoutElement.hidden);
      });

      // Reset full capabilities
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);
      expectEquals(false, layoutElement.hidden);

      // Test with PDF - should be hidden.
      initDocumentInfo(true, false);
      expectEquals(true, layoutElement.hidden);
    });

    test(assert(TestNames.Color), function() {
      const colorElement = page.$$('print-preview-color-settings');
      expectEquals(false, colorElement.hidden);

      // Remove color capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.color;

      // Each of these settings should not show the capability.
      [null, {option: [{type: 'STANDARD_COLOR', is_default: true}]}, {
        option: [
          {type: 'STANDARD_COLOR', is_default: true}, {type: 'CUSTOM_COLOR'}
        ]
      },
       {
         option: [
           {type: 'STANDARD_MONOCHROME', is_default: true},
           {type: 'CUSTOM_MONOCHROME'}
         ]
       },
       {option: [{type: 'STANDARD_MONOCHROME', is_default: true}]},
      ].forEach(colorCap => {
        capabilities =
            print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
        capabilities.printer.color = colorCap;
        // Layout section should now be hidden.
        page.set('destination_.capabilities', capabilities);
        expectEquals(true, colorElement.hidden);
      });

      // Custom color and monochrome options should make the section visible.
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      capabilities.printer.color =
        {option: [{ type: 'CUSTOM_COLOR', is_default: true },
                  { type: 'CUSTOM_MONOCHROME' }]};
      page.set('destination_.capabilities', capabilities);
      expectEquals(false, colorElement.hidden);
    });

    test(assert(TestNames.MediaSize), function() {
      const mediaSizeElement = page.$$('print-preview-media-size-settings');
      expectEquals(false, mediaSizeElement.hidden);

      // Remove capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.media_size;

      // Section should now be hidden.
      page.set('destination_.capabilities', capabilities);
      expectEquals(true, mediaSizeElement.hidden);

      // Reset
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);

      // Set PDF document type.
      initDocumentInfo(true, false);
      expectEquals(false, mediaSizeElement.hidden);

      // Set save as PDF. This should hide the settings section.
      setPdfDestination();
      expectEquals(true, mediaSizeElement.hidden);

      // Set HTML document type, should now show the section.
      initDocumentInfo(false, false);
      expectEquals(false, mediaSizeElement.hidden);
    });

    test(assert(TestNames.Margins), function() {
      const marginsElement = page.$$('print-preview-margins-settings');

      // Section is available for HTML (modifiable) documents
      initDocumentInfo(false, false);
      expectEquals(false, marginsElement.hidden);

      // Unavailable for PDFs.
      initDocumentInfo(true, false);
      expectEquals(true, marginsElement.hidden);
    });

    test(assert(TestNames.Dpi), function() {
      const dpiElement = page.$$('print-preview-dpi-settings');
      expectEquals(false, dpiElement.hidden);

      // Remove capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.dpi;

      // Section should now be hidden.
      page.set('destination_.capabilities', capabilities);
      expectEquals(true, dpiElement.hidden);

      // Does not show up for only 1 option.
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      capabilities.printer.dpi.option.pop();
      page.set('destination_.capabilities', capabilities);
      expectEquals(true, dpiElement.hidden);
    });

    test(assert(TestNames.Scaling), function() {
      const scalingElement = page.$$('print-preview-scaling-settings');
      expectEquals(false, scalingElement.hidden);

      // HTML to non-PDF destination -> only input shown
      initDocumentInfo(false, false);
      const fitToPageContainer = scalingElement.$$('.checkbox');
      const scalingInput =
          scalingElement.$$('print-preview-number-settings-section')
              .$$('.user-value');
      expectEquals(false, scalingElement.hidden);
      expectEquals(true, fitToPageContainer.hidden);
      expectEquals(false, scalingInput.hidden);

      // PDF to non-PDF destination -> checkbox and input shown.
      initDocumentInfo(true, false);
      expectEquals(false, scalingElement.hidden);
      expectEquals(false, fitToPageContainer.hidden);
      expectEquals(false, scalingInput.hidden);

      // PDF to PDF destination -> section disappears.
      setPdfDestination();
      expectEquals(true, scalingElement.hidden);
    });

    test(assert(TestNames.Other), function() {
      const optionsElement = page.$$('print-preview-other-options-settings');
      const headerFooter = optionsElement.$$('#header-footer').parentElement;
      const duplex = optionsElement.$$('#duplex').parentElement;
      const cssBackground = optionsElement.$$('#css-background').parentElement;
      const rasterize = optionsElement.$$('#rasterize').parentElement;
      const selectionOnly = optionsElement.$$('#selection-only').parentElement;

      // Start with HTML + duplex capability.
      initDocumentInfo(false, false);
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);
      expectEquals(false, optionsElement.hidden);
      expectEquals(false, headerFooter.hidden);
      expectEquals(false, duplex.hidden);
      expectEquals(false, cssBackground.hidden);
      expectEquals(true, rasterize.hidden);
      expectEquals(true, selectionOnly.hidden);

      // Add a selection - should show selection only.
      initDocumentInfo(false, true);
      expectEquals(false, optionsElement.hidden);
      expectEquals(false, selectionOnly.hidden);

      // Remove duplex capability.
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.duplex;
      page.set('destination_.capabilities', capabilities);
      Polymer.dom.flush();
      expectEquals(false, optionsElement.hidden);
      expectEquals(true, duplex.hidden);

      // PDF
      initDocumentInfo(true, false);
      Polymer.dom.flush();
      expectEquals(cr.isWindows || cr.isMac, optionsElement.hidden);
      expectEquals(true, headerFooter.hidden);
      expectEquals(true, duplex.hidden);
      expectEquals(true, cssBackground.hidden);
      expectEquals(cr.isWindows || cr.isMac, rasterize.hidden);
      expectEquals(true, selectionOnly.hidden);

      // Add a selection - should do nothing for PDFs.
      initDocumentInfo(true, true);
      expectEquals(cr.isWindows || cr.isMac, optionsElement.hidden);
      expectEquals(true, selectionOnly.hidden);

      // Add duplex.
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);
      Polymer.dom.flush();
      expectEquals(false, optionsElement.hidden);
      expectEquals(false, duplex.hidden);
    });

    test(assert(TestNames.SetPages), function() {
      const pagesElement = page.$$('print-preview-pages-settings');
      // This section is always visible.
      expectEquals(false, pagesElement.hidden);

      // Default value is all pages. Print ticket expects this to be empty.
      const allRadio = pagesElement.$$('#all-radio-button');
      const customRadio = pagesElement.$$('#custom-radio-button');
      const pagesInput = pagesElement.$.pageSettingsCustomInput;
      const hint = pagesElement.$$('.hint');

      /**
       * @param {boolean} allChecked Whether the all pages radio button is
       *     selected.
       * @param {string} inputString The expected string in the pages input.
       * @param {boolean} valid Whether the input string is valid.
       */
      const validateInputState = function(allChecked, inputString, valid) {
        expectEquals(allChecked, allRadio.checked);
        expectEquals(!allChecked, customRadio.checked);
        expectEquals(inputString, pagesInput.value);
        expectEquals(valid, hint.hidden);
      };
      validateInputState(true, '', true);
      expectEquals(0, page.settings.ranges.value.length);
      expectEquals(3, page.settings.pages.value.length);
      expectEquals(true, page.settings.pages.valid);

      // Set selection of pages 1 and 2.
      customRadio.checked = true;
      allRadio.dispatchEvent(new CustomEvent('change'));
      pagesInput.value = '1-2';
      pagesInput.dispatchEvent(new CustomEvent('input'));
      return test_util.eventToPromise('input-change', pagesElement).then(
          function() {
        validateInputState(false, '1-2', true);
        assertEquals(1, page.settings.ranges.value.length);
        expectEquals(1, page.settings.ranges.value[0].from);
        expectEquals(2, page.settings.ranges.value[0].to);
        expectEquals(2, page.settings.pages.value.length);
        expectEquals(true, page.settings.pages.valid);

        // Select pages 1 and 3
        pagesInput.value = '1, 3';
        pagesInput.dispatchEvent(new CustomEvent('input'));
        return test_util.eventToPromise('input-change', pagesElement);
      }).then(function() {
        validateInputState(false, '1, 3', true);
        assertEquals(2, page.settings.ranges.value.length);
        expectEquals(1, page.settings.ranges.value[0].from);
        expectEquals(1, page.settings.ranges.value[0].to);
        expectEquals(3, page.settings.ranges.value[1].from);
        expectEquals(3, page.settings.ranges.value[1].to);
        expectEquals(2, page.settings.pages.value.length);
        expectEquals(true, page.settings.pages.valid);

        // Enter an out of bounds value.
        pagesInput.value = '5';
        pagesInput.dispatchEvent(new CustomEvent('input'));
        return test_util.eventToPromise('input-change', pagesElement);
      }).then(function() {
        // Now the pages settings value should be invalid, and the error
        // message should be displayed.
        validateInputState(false, '5', false);
        expectEquals(false, page.settings.pages.valid);
      });
    });

    test(assert(TestNames.SetCopies), function() {
      const copiesElement = page.$$('print-preview-copies-settings');
      expectEquals(false, copiesElement.hidden);

      // Default value is 1
      const copiesInput =
          copiesElement.$$('print-preview-number-settings-section')
              .$$('.user-value');
      expectEquals('1', copiesInput.value);
      expectEquals(1, page.settings.copies.value);

      // Change to 2
      copiesInput.value = '2';
      copiesInput.dispatchEvent(new CustomEvent('input'));
      return test_util.eventToPromise('input-change', copiesElement).then(
          function() {
        expectEquals(2, page.settings.copies.value);

        // Collate is true by default.
        const collateInput = copiesElement.$.collate;
        expectEquals(true, collateInput.checked);
        expectEquals(true, page.settings.collate.value);

        // Uncheck the box.
        MockInteractions.tap(collateInput);
        expectEquals(false, collateInput.checked);
        collateInput.dispatchEvent(new CustomEvent('change'));
        expectEquals(false, page.settings.collate.value);
      });
    });

    test(assert(TestNames.SetLayout), function() {
      const layoutElement = page.$$('print-preview-layout-settings');
      expectEquals(false, layoutElement.hidden);

      // Default is portrait
      const layoutInput = layoutElement.$$('select');
      expectEquals('portrait', layoutInput.value);
      expectEquals(false, page.settings.layout.value);

      // Change to landscape
      layoutInput.value = 'landscape';
      layoutInput.dispatchEvent(new CustomEvent('change'));
      expectEquals(true, page.settings.layout.value);
    });

    test(assert(TestNames.SetColor), function() {
      const colorElement = page.$$('print-preview-color-settings');
      expectEquals(false, colorElement.hidden);

      // Default is color
      const colorInput = colorElement.$$('select');
      expectEquals('color', colorInput.value);
      expectEquals(true, page.settings.color.value);

      // Change to black and white.
      colorInput.value = 'bw';
      colorInput.dispatchEvent(new CustomEvent('change'));
      expectEquals(false, page.settings.color.value);
    });

    test(assert(TestNames.SetMediaSize), function() {
      const mediaSizeElement = page.$$('print-preview-media-size-settings');
      expectEquals(false, mediaSizeElement.hidden);

      const mediaSizeCapabilities =
          page.destination_.capabilities.printer.media_size;
      const letterOption = JSON.stringify(mediaSizeCapabilities.option[0]);
      const squareOption = JSON.stringify(mediaSizeCapabilities.option[1]);

      // Default is letter
      const mediaSizeInput =
          mediaSizeElement.$$('print-preview-settings-select').$$('select');
      expectEquals(letterOption, mediaSizeInput.value);
      expectEquals(letterOption, JSON.stringify(page.settings.mediaSize.value));

      // Change to square
      mediaSizeInput.value = squareOption;
      mediaSizeInput.dispatchEvent(new CustomEvent('change'));

      expectEquals(squareOption, JSON.stringify(page.settings.mediaSize.value));
    });

    test(assert(TestNames.SetDpi), function() {
      const dpiElement = page.$$('print-preview-dpi-settings');
      expectEquals(false, dpiElement.hidden);

      const dpiCapabilities = page.destination_.capabilities.printer.dpi;
      const highQualityOption = dpiCapabilities.option[0];
      const lowQualityOption = dpiCapabilities.option[1];

      // Default is 200
      const dpiInput =
          dpiElement.$$('print-preview-settings-select').$$('select');
      const isDpiEqual = function(value1, value2) {
        return value1.horizontal_dpi == value2.horizontal_dpi &&
            value1.vertical_dpi == value2.vertical_dpi &&
            value1.vendor_id == value2.vendor_id;
      };
      expectTrue(isDpiEqual(highQualityOption, JSON.parse(dpiInput.value)));
      expectTrue(isDpiEqual(highQualityOption, page.settings.dpi.value));

      // Change to 100
      dpiInput.value =
          JSON.stringify(dpiElement.capabilityWithLabels_.option[1]);
      dpiInput.dispatchEvent(new CustomEvent('change'));

      expectTrue(isDpiEqual(lowQualityOption, JSON.parse(dpiInput.value)));
      expectTrue(isDpiEqual(lowQualityOption, page.settings.dpi.value));
    });

    test(assert(TestNames.SetMargins), function() {
      const marginsElement = page.$$('print-preview-margins-settings');
      expectEquals(false, marginsElement.hidden);

      // Default is DEFAULT_MARGINS
      const marginsInput = marginsElement.$$('select');
      expectEquals(
          print_preview.ticket_items.MarginsTypeValue.DEFAULT.toString(),
          marginsInput.value);
      expectEquals(
          print_preview.ticket_items.MarginsTypeValue.DEFAULT,
          page.settings.margins.value);

      // Change to minimum.
      marginsInput.value =
          print_preview.ticket_items.MarginsTypeValue.MINIMUM.toString();
      marginsInput.dispatchEvent(new CustomEvent('change'));
      expectEquals(
          print_preview.ticket_items.MarginsTypeValue.MINIMUM,
          page.settings.margins.value);
    });

    test(assert(TestNames.SetScaling), function() {
      const scalingElement = page.$$('print-preview-scaling-settings');

      // Default is 100
      const scalingInput =
          scalingElement.$$('print-preview-number-settings-section')
              .$$('input');
      const fitToPageCheckbox =
          scalingElement.$$('#fit-to-page-checkbox');

      const validateScalingState = (scalingValue, scalingValid, fitToPage) => {
        // Invalid scalings are always set directly in the input, so no need to
        // verify that the input matches them.
        if (scalingValid) {
          const scalingDisplay =
              fitToPage ? page.documentInfo_.fitToPageScaling.toString() :
              scalingValue;
          expectEquals(scalingDisplay, scalingInput.value);
        }
        expectEquals(scalingValue, page.settings.scaling.value);
        expectEquals(scalingValid, page.settings.scaling.valid);
        expectEquals(fitToPage, fitToPageCheckbox.checked);
        expectEquals(fitToPage, page.settings.fitToPage.value);
      };

      // Set PDF so both scaling and fit to page are active.
      initDocumentInfo(true, false);
      expectEquals(false, scalingElement.hidden);

      // Default is 100
      validateScalingState('100', true, false);

      // Change to 105
      scalingInput.value = '105';
      scalingInput.dispatchEvent(new CustomEvent('input'));
      return test_util.eventToPromise('input-change', scalingElement).then(
          function() {
        validateScalingState('105', true, false);

        // Change to fit to page. Should display fit to page scaling but not
        // alter the scaling setting.
        fitToPageCheckbox.checked = true;
        fitToPageCheckbox.dispatchEvent(new CustomEvent('change'));
        validateScalingState('105', true, true);

        // Set scaling. Should uncheck fit to page and set the settings for
        // scaling and fit to page.
        scalingInput.value = '95';
        scalingInput.dispatchEvent(new CustomEvent('input'));
        return test_util.eventToPromise('input-change', scalingElement);
      }).then(function() {
        validateScalingState('95', true, false);

        // Set scaling to something invalid. Should change setting validity but
        // not value.
        scalingInput.value = '5';
        scalingInput.dispatchEvent(new CustomEvent('input'));
        return test_util.eventToPromise('input-change', scalingElement);
      }).then(function() {
        validateScalingState('95', false, false);

        // Check fit to page. Should set scaling valid.
        fitToPageCheckbox.checked = true;
        fitToPageCheckbox.dispatchEvent(new CustomEvent('change'));
        validateScalingState('95', true, true);

        // Uncheck fit to page. Should reset scaling to last valid.
        fitToPageCheckbox.checked = false;
        fitToPageCheckbox.dispatchEvent(new CustomEvent('change'));
        validateScalingState('95', true, false);
      });
    });

    test(assert(TestNames.SetOther), function() {
      const optionsElement = page.$$('print-preview-other-options-settings');
      expectEquals(false, optionsElement.hidden);

      // HTML - Header/footer, duplex, and CSS background. Also add selection.
      initDocumentInfo(false, true);

      const testOptionCheckbox = (element, defaultValue, optionSetting) => {
        expectEquals(false, element.hidden);
        expectEquals(defaultValue, element.checked);
        expectEquals(defaultValue, optionSetting.value);
        element.checked = !defaultValue;
        element.dispatchEvent(new CustomEvent('change'));
        expectEquals(!defaultValue, optionSetting.value);
      };

      // Check HTML settings
      const ids =
          ['header-footer', 'duplex', 'css-background', 'selection-only'];
      const defaults = [true, true, false, false];
      const optionSettings =
          [page.settings.headerFooter, page.settings.duplex,
           page.settings.cssBackground, page.settings.selectionOnly];

      optionSettings.forEach((option, index) => {
        testOptionCheckbox(
            optionsElement.$$('#' + ids[index]), defaults[index],
            optionSettings[index]);
      });

      // Set PDF to test rasterize
      if (!cr.isWindows && !cr.isMac) {
        initDocumentInfo(true, false);
        testOptionCheckbox(
            optionsElement.$$('#rasterize'), false, page.settings.rasterize);
      }
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
