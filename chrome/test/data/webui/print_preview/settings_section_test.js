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
    HeaderFooter: 'header footer',
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
      const initialSettings =
          print_preview_test_utils.getDefaultInitialSettings();
      const nativeLayer = new print_preview.NativeLayerStub();
      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getCddTemplate(initialSettings.printerName));
      nativeLayer.setPageCount(3);
      print_preview.NativeLayer.setInstance(nativeLayer);
      PolymerTest.clearBody();
      page = document.createElement('print-preview-app');
      const previewArea = page.$$('print-preview-preview-area');
      previewArea.plugin_ = new print_preview.PDFPluginStub(
          previewArea.onPluginLoad_.bind(previewArea));
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

    function toggleMoreSettings() {
      const moreSettingsElement = page.$$('print-preview-more-settings');
      moreSettingsElement.$.label.click();
    }

    test(assert(TestNames.Copies), function() {
      const copiesElement = page.$$('print-preview-copies-settings');
      assertFalse(copiesElement.hidden);

      // Remove copies capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.copies;

      // Copies section should now be hidden.
      page.set('destination_.capabilities', capabilities);
      assertTrue(copiesElement.hidden);
    });

    test(assert(TestNames.Layout), function() {
      const layoutElement = page.$$('print-preview-layout-settings');

      // Set up with HTML document. No selection.
      initDocumentInfo(false, false);
      assertFalse(layoutElement.hidden);

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
        assertTrue(layoutElement.hidden);
      });

      // Reset full capabilities
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);
      assertFalse(layoutElement.hidden);

      // Test with PDF - should be hidden.
      initDocumentInfo(true, false);
      assertTrue(layoutElement.hidden);
    });

    test(assert(TestNames.Color), function() {
      const colorElement = page.$$('print-preview-color-settings');
      assertFalse(colorElement.hidden);

      // Remove color capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.color;

      // Each of these settings should not show the capability. The value should
      // be the default for settings with multiple options and the only
      // available option otherwise.
      [
        {
          colorCap: null,
          expectedValue: false,
        },
        {
          colorCap: {option: [{type: 'STANDARD_COLOR', is_default: true}]},
          expectedValue: true,
        },
        {
          colorCap: { option: [{type: 'STANDARD_COLOR', is_default: true},
                               {type: 'CUSTOM_COLOR'}]},
          expectedValue: true,
        },
        {
          colorCap: { option: [{type: 'STANDARD_MONOCHROME', is_default: true},
                               {type: 'CUSTOM_MONOCHROME'}]},
          expectedValue: false,
        },
        {
          colorCap: {option: [{type: 'STANDARD_MONOCHROME'}]},
          expectedValue: false,
        },
        {
          colorCap: {option: [{type: 'CUSTOM_MONOCHROME', vendor_id: '42'}]},
          expectedValue: false,
        },
        {
          colorCap: {option: [{type: 'CUSTOM_COLOR', vendor_id: '42'}]},
          expectedValue: true,
        }
      ].forEach(capabilityAndValue => {
        capabilities =
            print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
        capabilities.printer.color = capabilityAndValue.colorCap;
        // Layout section should now be hidden.
        page.set('destination_.capabilities', capabilities);
        assertTrue(colorElement.hidden);
        assertEquals(capabilityAndValue.expectedValue,
                     page.getSettingValue('color'));
      });

      // Each of these settings should show the capability with the default
      // value given by expectedValue.
      [
        {
          colorCap: {
            option: [{type: 'STANDARD_MONOCHROME', is_default: true},
                     {type: 'STANDARD_COLOR'}]
          },
          expectedValue: false,
        },
        {
          colorCap: {
            option: [{type: 'STANDARD_MONOCHROME'},
                     {type: 'STANDARD_COLOR', is_default: true}]
          },
          expectedValue: true,
        },
        {
          colorCap: {
            option: [
              {type: 'CUSTOM_MONOCHROME', vendor_id: '42'},
              {type: 'CUSTOM_COLOR', is_default: true, vendor_id: '43'}
            ]
          },
          expectedValue: true,
        }
      ].forEach(capabilityAndValue => {
        capabilities =
            print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
        capabilities.printer.color = capabilityAndValue.colorCap;
        page.set('destination_.capabilities', capabilities);
        assertFalse(colorElement.hidden);
        assertEquals(capabilityAndValue.expectedValue ? 'color' : 'bw',
                     colorElement.$$('select').value);
        assertEquals(capabilityAndValue.expectedValue,
                     page.getSettingValue('color'));
      });
    });

    test(assert(TestNames.MediaSize), function() {
      const mediaSizeElement = page.$$('print-preview-media-size-settings');
      assertTrue(mediaSizeElement.hidden);

      // Expand more settings to reveal the element.
      toggleMoreSettings();
      assertFalse(mediaSizeElement.hidden);

      // Remove capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.media_size;

      // Section should now be hidden.
      page.set('destination_.capabilities', capabilities);
      assertTrue(mediaSizeElement.hidden);

      // Reset
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);

      // Set PDF document type.
      initDocumentInfo(true, false);
      assertFalse(mediaSizeElement.hidden);

      // Set save as PDF. This should hide the settings section.
      setPdfDestination();
      assertTrue(mediaSizeElement.hidden);

      // Set HTML document type, should now show the section.
      initDocumentInfo(false, false);
      assertFalse(mediaSizeElement.hidden);
    });

    test(assert(TestNames.Margins), function() {
      const marginsElement = page.$$('print-preview-margins-settings');

      // Section is available for HTML (modifiable) documents
      initDocumentInfo(false, false);
      assertTrue(marginsElement.hidden);

      // Expand more settings to reveal the element.
      toggleMoreSettings();
      assertFalse(marginsElement.hidden);

      // Unavailable for PDFs.
      initDocumentInfo(true, false);
      assertTrue(marginsElement.hidden);
    });

    test(assert(TestNames.Dpi), function() {
      const dpiElement = page.$$('print-preview-dpi-settings');
      assertTrue(dpiElement.hidden);

      // Expand more settings to reveal the element.
      toggleMoreSettings();
      assertFalse(dpiElement.hidden);

      // Remove capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.dpi;

      // Section should now be hidden.
      page.set('destination_.capabilities', capabilities);
      assertTrue(dpiElement.hidden);

      // Does not show up for only 1 option.
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      capabilities.printer.dpi.option.pop();
      page.set('destination_.capabilities', capabilities);
      assertTrue(dpiElement.hidden);
    });

    test(assert(TestNames.Scaling), function() {
      const scalingElement = page.$$('print-preview-scaling-settings');
      assertTrue(scalingElement.hidden);

      // Expand more settings to reveal the element.
      toggleMoreSettings();
      assertFalse(scalingElement.hidden);

      // HTML to non-PDF destination -> only input shown
      initDocumentInfo(false, false);
      const fitToPageContainer = scalingElement.$$('.checkbox');
      const scalingInputWrapper =
          scalingElement.$$('print-preview-number-settings-section')
              .$$('.input-wrapper');
      assertFalse(scalingElement.hidden);
      assertTrue(fitToPageContainer.hidden);
      assertFalse(scalingInputWrapper.hidden);

      // PDF to non-PDF destination -> checkbox and input shown. Check that if
      // more settings is collapsed the section is hidden.
      initDocumentInfo(true, false);
      toggleMoreSettings();
      assertTrue(scalingElement.hidden);
      toggleMoreSettings();
      assertFalse(scalingElement.hidden);
      assertFalse(fitToPageContainer.hidden);
      assertFalse(scalingInputWrapper.hidden);

      // PDF to PDF destination -> section disappears.
      setPdfDestination();
      assertTrue(scalingElement.hidden);
    });

    test(assert(TestNames.Other), function() {
      const optionsElement = page.$$('print-preview-other-options-settings');
      const headerFooter = optionsElement.$.headerFooter.parentElement;
      const duplex = optionsElement.$.duplex.parentElement;
      const cssBackground = optionsElement.$.cssBackground.parentElement;
      const rasterize = optionsElement.$.rasterize.parentElement;
      const selectionOnly = optionsElement.$.selectionOnly.parentElement;

      // Start with HTML + duplex capability.
      initDocumentInfo(false, false);
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);
      assertTrue(optionsElement.hidden);

      // Expanding more settings will show the section.
      toggleMoreSettings();
      assertFalse(headerFooter.hidden);
      assertFalse(duplex.hidden);
      assertFalse(cssBackground.hidden);
      assertTrue(rasterize.hidden);
      assertTrue(selectionOnly.hidden);

      // Add a selection - should show selection only.
      initDocumentInfo(false, true);
      assertFalse(optionsElement.hidden);
      assertFalse(selectionOnly.hidden);

      // Remove duplex capability.
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.duplex;
      page.set('destination_.capabilities', capabilities);
      Polymer.dom.flush();
      assertFalse(optionsElement.hidden);
      assertTrue(duplex.hidden);

      // PDF
      initDocumentInfo(true, false);
      Polymer.dom.flush();
      if (cr.isWindows || cr.isMac) {
        // No options
        assertTrue(optionsElement.hidden);
      } else {
        // All sections hidden except rasterize
        assertTrue(headerFooter.hidden);
        assertTrue(duplex.hidden);
        assertTrue(cssBackground.hidden);
        expectEquals(cr.isWindows || cr.isMac, rasterize.hidden);
        assertTrue(selectionOnly.hidden);
      }

      // Add a selection - should do nothing for PDFs.
      initDocumentInfo(true, true);
      expectEquals(cr.isWindows || cr.isMac, optionsElement.hidden);
      assertTrue(selectionOnly.hidden);

      // Add duplex.
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);
      Polymer.dom.flush();
      assertFalse(optionsElement.hidden);
      assertFalse(duplex.hidden);
    });

    test(assert(TestNames.HeaderFooter), function() {
      const optionsElement = page.$$('print-preview-other-options-settings');
      const headerFooter = optionsElement.$.headerFooter.parentElement;

      // HTML page to show Header/Footer option.
      initDocumentInfo(false, false);
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      page.set('destination_.capabilities', capabilities);
      assertTrue(optionsElement.hidden);

      toggleMoreSettings();
      assertFalse(optionsElement.hidden);
      assertFalse(headerFooter.hidden);

      // Set margins to NONE
      page.set('settings.margins.value',
               print_preview.ticket_items.MarginsTypeValue.NO_MARGINS);
      assertTrue(headerFooter.hidden);

      // Custom margins of 0.
      page.set('settings.margins.value',
               print_preview.ticket_items.MarginsTypeValue.CUSTOM);
      page.set('settings.customMargins.vaue',
               {marginTop: 0, marginLeft: 0, marginRight: 0, marginBottom: 0});
      assertTrue(headerFooter.hidden);

      // Custom margins of 36 -> header/footer available
      page.set(
          'settings.customMargins.value',
          {marginTop: 36, marginLeft: 36, marginRight: 36, marginBottom: 36});
      assertFalse(headerFooter.hidden);

      // Zero top and bottom -> header/footer unavailable
      page.set(
          'settings.customMargins.value',
          {marginTop: 0, marginLeft: 36, marginRight: 36, marginBottom: 0});
      assertTrue(headerFooter.hidden);

      // Zero top and nonzero bottom -> header/footer available
      page.set(
          'settings.customMargins.value',
          {marginTop: 0, marginLeft: 36, marginRight: 36, marginBottom: 36});
      assertFalse(headerFooter.hidden);

      // Small paper sizes
      capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      capabilities.printer.media_size = {
        'option': [
          {'name': 'SmallLabel', 'width_microns': 38100,
            'height_microns': 12700, 'is_default': false},
          {'name': 'BigLabel', 'width_microns': 50800,
            'height_microns': 76200, 'is_default': true}
        ]
      };
      page.set('destination_.capabilities', capabilities);
      page.set('settings.margins.value',
               print_preview.ticket_items.MarginsTypeValue.DEFAULT);

      // Header/footer should be available for default big label
      assertFalse(headerFooter.hidden);

      page.set(
          'settings.mediaSize.value',
          capabilities.printer.media_size.option[0]);

      // Header/footer should not be available for small label
      assertTrue(headerFooter.hidden);
    });

    test(assert(TestNames.SetPages), function() {
      const pagesElement = page.$$('print-preview-pages-settings');
      // This section is always visible.
      assertFalse(pagesElement.hidden);

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
      assertTrue(page.settings.pages.valid);

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
        assertTrue(page.settings.pages.valid);

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
        assertTrue(page.settings.pages.valid);

        // Enter an out of bounds value.
        pagesInput.value = '5';
        pagesInput.dispatchEvent(new CustomEvent('input'));
        return test_util.eventToPromise('input-change', pagesElement);
      }).then(function() {
        // Now the pages settings value should be invalid, and the error
        // message should be displayed.
        validateInputState(false, '5', false);
        assertFalse(page.settings.pages.valid);
      });
    });

    test(assert(TestNames.SetCopies), function() {
      const copiesElement = page.$$('print-preview-copies-settings');
      assertFalse(copiesElement.hidden);

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
        assertTrue(collateInput.checked);
        assertTrue(page.settings.collate.value);

        // Uncheck the box.
        MockInteractions.tap(collateInput);
        assertFalse(collateInput.checked);
        collateInput.dispatchEvent(new CustomEvent('change'));
        assertFalse(page.settings.collate.value);
      });
    });

    test(assert(TestNames.SetLayout), function() {
      const layoutElement = page.$$('print-preview-layout-settings');
      assertFalse(layoutElement.hidden);

      // Default is portrait
      const layoutInput = layoutElement.$$('select');
      expectEquals('portrait', layoutInput.value);
      assertFalse(page.settings.layout.value);

      // Change to landscape
      layoutInput.value = 'landscape';
      layoutInput.dispatchEvent(new CustomEvent('change'));
      assertTrue(page.settings.layout.value);
    });

    test(assert(TestNames.SetColor), function() {
      const colorElement = page.$$('print-preview-color-settings');
      assertFalse(colorElement.hidden);

      // Default is color
      const colorInput = colorElement.$$('select');
      expectEquals('color', colorInput.value);
      assertTrue(page.settings.color.value);

      // Change to black and white.
      colorInput.value = 'bw';
      colorInput.dispatchEvent(new CustomEvent('change'));
      assertFalse(page.settings.color.value);
    });

    test(assert(TestNames.SetMediaSize), function() {
      toggleMoreSettings();
      const mediaSizeElement = page.$$('print-preview-media-size-settings');
      assertFalse(mediaSizeElement.hidden);

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
      toggleMoreSettings();
      const dpiElement = page.$$('print-preview-dpi-settings');
      assertFalse(dpiElement.hidden);

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
      toggleMoreSettings();
      const marginsElement = page.$$('print-preview-margins-settings');
      assertFalse(marginsElement.hidden);

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
      toggleMoreSettings();
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
      assertFalse(scalingElement.hidden);

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
      toggleMoreSettings();
      const optionsElement = page.$$('print-preview-other-options-settings');
      assertFalse(optionsElement.hidden);

      // HTML - Header/footer, duplex, and CSS background. Also add selection.
      initDocumentInfo(false, true);

      const testOptionCheckbox = (element, defaultValue, optionSetting) => {
        assertFalse(element.hidden);
        expectEquals(defaultValue, element.checked);
        expectEquals(defaultValue, optionSetting.value);
        element.checked = !defaultValue;
        element.dispatchEvent(new CustomEvent('change'));
        expectEquals(!defaultValue, optionSetting.value);
      };

      // Check HTML settings
      const ids =
          ['headerFooter', 'duplex', 'cssBackground', 'selectionOnly'];
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
