// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_sections_tests', function() {
  /** @enum {string} */
  const TestNames = {
    SettingsSectionsVisibilityChange: 'settings sections visibility change',
    SetLayout: 'set layout',
    SetColor: 'set color',
    SetMargins: 'set margins',
    SetPagesPerSheet: 'set pages per sheet',
    PresetCopies: 'preset copies',
    PresetDuplex: 'preset duplex',
    DisableMarginsByPagesPerSheet: 'disable margins by pages per sheet',
    ColorManaged: 'color managed',
    DuplexManaged: 'duplex managed',
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
      const pluginProxy = new print_preview.PDFPluginStub();
      print_preview_new.PluginProxy.setInstance(pluginProxy);

      PolymerTest.clearBody();
      page = document.createElement('print-preview-app');
      document.body.appendChild(page);
      const previewArea = page.$.previewArea;
      pluginProxy.setLoadCallback(previewArea.onPluginLoad_.bind(previewArea));

      // Wait for initialization to complete.
      return Promise
          .all([
            nativeLayer.whenCalled('getInitialSettings'),
            nativeLayer.whenCalled('getPrinterCapabilities')
          ])
          .then(function() {
            Polymer.dom.flush();
          });
    });

    /**
     * @param {boolean} isPdf Whether the document should be a PDF.
     * @param {boolean} hasSelection Whether the document has a selection.
     */
    function initDocumentInfo(isPdf, hasSelection) {
      const info = page.$.documentInfo;
      info.init(!isPdf, 'title', hasSelection);
      if (isPdf) {
        info.set('documentSettings.fitToPageScaling', 98);
      }
      info.set('documentSettings.pageCount', 3);
      info.margins = null;
      Polymer.dom.flush();
    }

    function addSelection() {
      // Add a selection.
      page.$.documentInfo.init(
          page.documentSettings_.isModifiable, 'title', true);
      Polymer.dom.flush();
    }

    function setPdfDestination() {
      const saveAsPdfDestination =
          print_preview_test_utils.getSaveAsPdfDestination();
      saveAsPdfDestination.capabilities =
          print_preview_test_utils.getCddTemplate(saveAsPdfDestination.id)
              .capabilities;
      page.set('destination_', saveAsPdfDestination);
    }

    function toggleMoreSettings() {
      const moreSettingsElement = page.$$('print-preview-more-settings');
      moreSettingsElement.$.label.click();
    }

    test(assert(TestNames.SettingsSectionsVisibilityChange), function() {
      toggleMoreSettings();
      const camelToKebab = s => s.replace(/([A-Z])/g, '-$1').toLowerCase();
      ['copies', 'layout', 'color', 'mediaSize', 'margins', 'dpi', 'scaling',
       'otherOptions']
          .forEach(setting => {
            const element =
                page.$$(`print-preview-${camelToKebab(setting)}-settings`);
            // Show, hide and reset.
            [true, false, true].forEach(value => {
              page.set(`settings.${setting}.available`, value);
              // Element expected to be visible when available.
              assertEquals(!value, element.hidden);
            });
          });
    });

    test(assert(TestNames.SetLayout), function() {
      const layoutElement = page.$$('print-preview-layout-settings');
      assertFalse(layoutElement.hidden);

      // Default is portrait
      const layoutInput = layoutElement.$$('select');
      assertEquals('portrait', layoutInput.value);
      assertFalse(page.settings.layout.value);

      // Change to landscape
      return print_preview_test_utils.selectOption(layoutElement, 'landscape')
          .then(function() {
            assertTrue(page.settings.layout.value);
          });
    });

    test(assert(TestNames.SetColor), function() {
      const colorElement = page.$$('print-preview-color-settings');
      assertFalse(colorElement.hidden);

      // Default is color
      const colorInput = colorElement.$$('select');
      assertEquals('color', colorInput.value);
      assertTrue(page.settings.color.value);

      // Change to black and white.
      return print_preview_test_utils.selectOption(colorElement, 'bw')
          .then(function() {
            assertFalse(page.settings.color.value);
          });
    });

    test(assert(TestNames.SetMargins), function() {
      toggleMoreSettings();
      const marginsElement = page.$$('print-preview-margins-settings');
      assertFalse(marginsElement.hidden);

      // Default is DEFAULT_MARGINS
      const marginsInput = marginsElement.$$('select');
      assertEquals(
          print_preview.ticket_items.MarginsTypeValue.DEFAULT.toString(),
          marginsInput.value);
      assertEquals(
          print_preview.ticket_items.MarginsTypeValue.DEFAULT,
          page.settings.margins.value);

      // Change to minimum.
      return print_preview_test_utils
          .selectOption(
              marginsElement,
              print_preview.ticket_items.MarginsTypeValue.MINIMUM.toString())
          .then(function() {
            assertEquals(
                print_preview.ticket_items.MarginsTypeValue.MINIMUM,
                page.settings.margins.value);
          });
    });

    test(assert(TestNames.SetPagesPerSheet), function() {
      toggleMoreSettings();
      const pagesPerSheetElement =
          page.$$('print-preview-pages-per-sheet-settings');
      assertFalse(pagesPerSheetElement.hidden);

      assertEquals('1', pagesPerSheetElement.$$('select').value);

      // Change to a different value
      return print_preview_test_utils.selectOption(pagesPerSheetElement, '2')
          .then(function() {
            assertEquals(2, page.settings.pagesPerSheet.value);
          });
    });

    // This test verifies that changing pages per sheet to N > 1 resets the
    // margins dropdown value to DEFAULT and disables it, and resetting
    // pages per sheet back to 1 re-enables the dropdown.
    test(assert(TestNames.DisableMarginsByPagesPerSheet), function() {
      toggleMoreSettings();
      const pagesPerSheetElement =
          page.$$('print-preview-pages-per-sheet-settings');
      assertFalse(pagesPerSheetElement.hidden);

      const pagesPerSheetInput = pagesPerSheetElement.$$('select');
      assertEquals(1, page.settings.pagesPerSheet.value);

      const marginsElement = page.$$('print-preview-margins-settings');
      assertFalse(marginsElement.hidden);

      const marginsTypeEnum = print_preview.ticket_items.MarginsTypeValue;

      // Default is DEFAULT_MARGINS
      const marginsInput = marginsElement.$$('select');
      assertEquals(marginsTypeEnum.DEFAULT, page.settings.margins.value);

      // Change margins to minimum.
      return print_preview_test_utils
          .selectOption(marginsElement, marginsTypeEnum.MINIMUM.toString())
          .then(function() {
            assertEquals(marginsTypeEnum.MINIMUM, page.settings.margins.value);
            assertFalse(marginsInput.disabled);
            // Change pages per sheet to a different value.
            return print_preview_test_utils.selectOption(
                pagesPerSheetElement, '2');
          })
          .then(function() {
            assertEquals(2, page.settings.pagesPerSheet.value);
            assertEquals(marginsTypeEnum.DEFAULT, page.settings.margins.value);
            assertEquals(
                marginsTypeEnum.DEFAULT.toString(), marginsInput.value);
            assertTrue(marginsInput.disabled);

            // Set pages per sheet back to 1.
            return print_preview_test_utils.selectOption(
                pagesPerSheetElement, '1');
          })
          .then(function() {
            assertEquals(1, page.settings.pagesPerSheet.value);
            assertEquals(marginsTypeEnum.DEFAULT, page.settings.margins.value);
            assertEquals(
                marginsTypeEnum.DEFAULT.toString(), marginsInput.value);
            assertFalse(marginsInput.disabled);
          });
    });

    test(assert(TestNames.PresetCopies), function() {
      const copiesElement = page.$$('print-preview-copies-settings');
      assertFalse(copiesElement.hidden);

      // Default value is 1
      const copiesInput =
          copiesElement.$$('print-preview-number-settings-section')
              .$.userValue.inputElement;
      assertEquals('1', copiesInput.value);
      assertEquals(1, page.settings.copies.value);

      // Send a preset value of 2
      const copies = 2;
      cr.webUIListenerCallback('print-preset-options', true, copies);
      assertEquals(copies, page.settings.copies.value);
      assertEquals(copies.toString(), copiesInput.value);
    });

    test(assert(TestNames.PresetDuplex), function() {
      toggleMoreSettings();
      const optionsElement = page.$$('print-preview-other-options-settings');
      assertFalse(optionsElement.hidden);

      // Default value is on, so turn it off
      page.setSetting('duplex', false);
      const checkbox = optionsElement.$$('#duplex');
      assertFalse(checkbox.checked);
      assertFalse(page.settings.duplex.value);

      // Send a preset value of LONG_EDGE
      const duplex = print_preview_new.DuplexMode.LONG_EDGE;
      cr.webUIListenerCallback('print-preset-options', false, 1, duplex);
      assertTrue(page.settings.duplex.value);
      assertTrue(checkbox.checked);
    });

    test(assert(TestNames.ColorManaged), function() {
      const colorElement = page.$$('print-preview-color-settings');
      assertFalse(colorElement.hidden);

      // Remove color capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.color;

      [{
        // Policy has no effect.
        colorCap: {option: [{type: 'STANDARD_COLOR', is_default: true}]},
        colorPolicy: print_preview.ColorMode.COLOR,
        colorDefault: print_preview.ColorMode.COLOR,
        expectedValue: true,
        expectedHidden: true,
        expectedManaged: false,
      },
       {
         // Policy contradicts actual capabilities and is ignored.
         colorCap: {option: [{type: 'STANDARD_COLOR', is_default: true}]},
         colorPolicy: print_preview.ColorMode.GRAY,
         colorDefault: print_preview.ColorMode.GRAY,
         expectedValue: true,
         expectedHidden: true,
         expectedManaged: false,
       },
       {
         // Policy overrides default.
         colorCap: {
           option: [
             {type: 'STANDARD_MONOCHROME', is_default: true},
             {type: 'STANDARD_COLOR'}
           ]
         },
         colorPolicy: print_preview.ColorMode.COLOR,
         // Default mismatches restriction and is ignored.
         colorDefault: print_preview.ColorMode.GRAY,
         expectedValue: true,
         expectedHidden: false,
         expectedManaged: true,
       },
       {
         // Default defined by policy but setting is modifiable.
         colorCap: {
           option: [
             {type: 'STANDARD_MONOCHROME', is_default: true},
             {type: 'STANDARD_COLOR'}
           ]
         },
         colorDefault: print_preview.ColorMode.COLOR,
         expectedValue: true,
         expectedHidden: false,
         expectedManaged: false,
       }].forEach(subtestParams => {
        capabilities =
            print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
        capabilities.printer.color = subtestParams.colorCap;
        const policies = {
          allowedColorModes: subtestParams.colorPolicy,
          defaultColorMode: subtestParams.colorDefault,
        };
        // In practice |capabilities| are always set after |policies| and
        // observers only check for |capabilities|, so the order is important.
        page.set('destination_.policies', policies);
        page.set('destination_.capabilities', capabilities);
        page.$$('print-preview-model').applyDestinationSpecificPolicies();
        assertEquals(subtestParams.expectedManaged, page.controlsManaged_);
        assertEquals(
            subtestParams.expectedValue, page.getSettingValue('color'));
        assertEquals(subtestParams.expectedHidden, colorElement.hidden);
        if (!subtestParams.expectedHidden) {
          const selectElement = colorElement.$$('select');
          assertEquals(
              subtestParams.expectedValue ? 'color' : 'bw',
              selectElement.value);
          assertEquals(subtestParams.expectedManaged, selectElement.disabled);
        }
      });
    });

    test(assert(TestNames.DuplexManaged), function() {
      toggleMoreSettings();
      const optionsElement = page.$$('print-preview-other-options-settings');
      const duplexElement = optionsElement.$$('#duplex');

      // Remove duplex capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
      delete capabilities.printer.duplex;

      [{
        // Policy has no effect.
        duplexCap: {option: [{type: 'NO_DUPLEX', is_default: true}]},
        duplexPolicy: print_preview.DuplexModeRestriction.SIMPLEX,
        duplexDefault: print_preview.DuplexModeRestriction.SIMPLEX,
        expectedValue: false,
        expectedHidden: true,
        expectedManaged: false,
      },
       {
         // Policy contradicts actual capabilities and is ignored.
         duplexCap: {option: [{type: 'NO_DUPLEX', is_default: true}]},
         duplexPolicy: print_preview.DuplexModeRestriction.DUPLEX,
         duplexDefault: print_preview.DuplexModeRestriction.LONG_EDGE,
         expectedValue: false,
         expectedHidden: true,
         expectedManaged: false,
       },
       {
         // Policy overrides default.
         duplexCap: {
           option: [
             {type: 'NO_DUPLEX', is_default: true}, {type: 'LONG_EDGE'},
             {type: 'SHORT_EDGE'}
           ]
         },
         duplexPolicy: print_preview.DuplexModeRestriction.DUPLEX,
         // Default mismatches restriction and is ignored.
         duplexDefault: print_preview.DuplexModeRestriction.SIMPLEX,
         expectedValue: true,
         expectedHidden: false,
         expectedManaged: true,
       },
       {
         // Default defined by policy but setting is modifiable.
         duplexCap: {
           option: [
             {type: 'NO_DUPLEX', is_default: true}, {type: 'LONG_EDGE'},
             {type: 'SHORT_EDGE'}
           ]
         },
         duplexDefault: print_preview.DuplexModeRestriction.LONG_EDGE,
         expectedValue: true,
         expectedHidden: false,
         expectedManaged: false,
       }].forEach(subtestParams => {
        capabilities =
            print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
        capabilities.printer.duplex = subtestParams.duplexCap;
        const policies = {
          allowedDuplexModes: subtestParams.duplexPolicy,
          defaultDuplexMode: subtestParams.duplexDefault,
        };
        // In practice |capabilities| are always set after |policies| and
        // observers only check for |capabilities|, so the order is important.
        page.set('destination_.policies', policies);
        page.set('destination_.capabilities', capabilities);
        page.$$('print-preview-model').applyDestinationSpecificPolicies();
        assertEquals(subtestParams.expectedManaged, page.controlsManaged_);
        assertEquals(
            subtestParams.expectedValue, page.getSettingValue('duplex'));
        assertEquals(
            subtestParams.expectedHidden,
            duplexElement.parentNode.parentNode.hidden);
        if (!subtestParams.expectedHidden) {
          assertEquals(subtestParams.expectedValue, duplexElement.checked);
          assertEquals(subtestParams.expectedManaged, duplexElement.disabled);
        }
      });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
