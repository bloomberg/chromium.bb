// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_sections_tests', function() {
  /** @enum {string} */
  const TestNames = {
    SettingsSectionsVisibilityChange: 'settings sections visibility change',
    PresetCopies: 'preset copies',
    PresetDuplex: 'preset duplex',
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
