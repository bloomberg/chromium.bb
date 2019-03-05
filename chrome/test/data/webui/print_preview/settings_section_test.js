// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_sections_tests', function() {
  /** @enum {string} */
  const TestNames = {
    SettingsSectionsVisibilityChange: 'settings sections visibility change',
    PresetCopies: 'preset copies',
    PresetDuplex: 'preset duplex',
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
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
