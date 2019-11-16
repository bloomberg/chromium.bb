// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloudPrintInterface, Destination, DuplexMode, NativeLayer, PluginProxy, setCloudPrintInterfaceForTesting} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {CloudPrintInterfaceStub} from 'chrome://test/print_preview/cloud_print_interface_stub.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {PDFPluginStub} from 'chrome://test/print_preview/plugin_stub.js';
import {getCddTemplate, getGoogleDriveDestination} from 'chrome://test/print_preview/print_preview_test_utils.js';

window.print_preview_app_test = {};
print_preview_app_test.suiteName = 'PrintPreviewAppTest';
/** @enum {string} */
print_preview_app_test.TestNames = {
  PrintToGoogleDrive: 'print to google drive',
  PrintPresets: 'print presets',
};

suite(print_preview_app_test.suiteName, function() {
  /** @type {?PrintPreviewAppElement} */
  let page = null;

  /** @type {?NativeLayer} */
  let nativeLayer = null;

  /** @type {?CloudPrintInterface} */
  let cloudPrintInterface = null;

  /** @type {?PluginProxy} */
  let pluginProxy = null;

  /** @type {!NativeInitialSettings} */
  const initialSettings = {
    isInKioskAutoPrintMode: false,
    isInAppKioskMode: false,
    thousandsDelimiter: ',',
    decimalDelimiter: '.',
    unitType: 1,
    previewModifiable: true,
    documentTitle: 'DocumentABC123',
    documentHasSelection: false,
    shouldPrintSelectionOnly: false,
    printerName: 'FooDevice',
    serializedAppStateStr: null,
    serializedDefaultDestinationSelectionRulesStr: null,
    pdfPrinterDisabled: false,
    cloudPrintURL: 'cloudprint URL',
    userAccounts: ['foo@chromium.org'],
  };

  /** @override */
  setup(function() {
    // Stub out the native layer, the cloud print interface, and the plugin.
    PolymerTest.clearBody();
    nativeLayer = new NativeLayerStub();
    NativeLayer.setInstance(nativeLayer);
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinationCapabilities(
        getCddTemplate(initialSettings.printerName));
    cloudPrintInterface = new CloudPrintInterfaceStub();
    setCloudPrintInterfaceForTesting(cloudPrintInterface);
    pluginProxy = new PDFPluginStub();
    PluginProxy.setInstance(pluginProxy);

    page = document.createElement('print-preview-app');
    document.body.appendChild(page);
    const previewArea = page.$.previewArea;
    return nativeLayer.whenCalled('getPreview');
  });

  // Regression test for https://crbug.com/936029
  test(
      assert(print_preview_app_test.TestNames.PrintToGoogleDrive), async () => {
        // Set up the UI to have Google Drive as the printer.
        page.destination_ = getGoogleDriveDestination('foo@chromium.org');
        page.destination_.capabilities = getCddTemplate(page.destination_.id);

        // Trigger print.
        const sidebar = page.$$('print-preview-sidebar');
        sidebar.dispatchEvent(new CustomEvent(
            'print-requested', {composed: true, bubbles: true}));

        // Validate arguments to cloud print interface.
        const args = await cloudPrintInterface.whenCalled('submit');
        assertEquals('sample data', args.data);
        assertEquals('DocumentABC123', args.documentTitle);
        assertEquals(Destination.GooglePromotedId.DOCS, args.destination.id);
        assertEquals('1.0', JSON.parse(args.printTicket).version);
      });

  test(assert(print_preview_app_test.TestNames.PrintPresets), function() {
    assertEquals(1, page.settings.copies.value);
    assertFalse(page.settings.duplex.value);

    // Send preset values of duplex LONG_EDGE and 2 copies.
    const copies = 2;
    const duplex = DuplexMode.LONG_EDGE;
    window.cr.webUIListenerCallback(
        'print-preset-options', true, copies, duplex);
    assertEquals(copies, page.getSettingValue('copies'));
    assertTrue(page.getSettingValue('duplex'));
    assertFalse(page.getSetting('duplex').setFromUi);
    assertFalse(page.getSetting('copies').setFromUi);
  });
});
