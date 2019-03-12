// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.CupsPrintersBrowserProxy} */
class TestCupsPrintersBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'addDiscoveredPrinter',
      'getCupsPrintersList',
      'getCupsPrinterManufacturersList',
      'getCupsPrinterModelsList',
      'getPrinterInfo',
      'getPrinterPpdManufacturerAndModel',
      'startDiscoveringPrinters',
      'stopDiscoveringPrinters',
      'cancelPrinterSetUp',
    ]);

    this.printerList = [];
    this.manufacturers = [];
    this.models = [];
    this.printerInfo = {};
    this.printerPpdMakeModel = {};
  }

  /** @override */
  addDiscoveredPrinter(printerId) {
    this.methodCalled('addDiscoveredPrinter', printerId);
  }

  /** @override */
  getCupsPrintersList() {
    this.methodCalled('getCupsPrintersList');
    return Promise.resolve(this.printerList);
  }

  /** @override */
  getCupsPrinterManufacturersList() {
    this.methodCalled('getCupsPrinterManufacturersList');
    return Promise.resolve(this.manufacturers);
  }

  /** @override */
  getCupsPrinterModelsList(manufacturer) {
    this.methodCalled('getCupsPrinterModelsList', manufacturer);
    return Promise.resolve(this.models);
  }

  /** @override */
  getPrinterInfo(newPrinter) {
    this.methodCalled('getPrinterInfo', newPrinter);
    return Promise.resolve(this.printerInfo);
  }

  /** @override */
  startDiscoveringPrinters() {
    this.methodCalled('startDiscoveringPrinters');
  }

  /** @override */
  stopDiscoveringPrinters() {
    this.methodCalled('stopDiscoveringPrinters');
  }

  /** @override */
  cancelPrinterSetUp(newPrinter) {
    this.methodCalled('cancelPrinterSetUp', newPrinter);
  }

  /** @override */
  getPrinterPpdManufacturerAndModel(printerId) {
    this.methodCalled('getPrinterPpdManufacturerAndModel', printerId);
    return Promise.resolve(this.printerPpdMakeModel);
  }
}

suite('CupsAddPrinterDialogTests', function() {
  function fillAddManuallyDialog(addDialog) {
    const name = addDialog.$$('#printerNameInput');
    const address = addDialog.$$('#printerAddressInput');

    assertTrue(!!name);
    name.value = 'Test printer';

    assertTrue(!!address);
    address.value = '127.0.0.1';

    const addButton = addDialog.$$('#addPrinterButton');
    assertTrue(!!addButton);
    assertFalse(addButton.disabled);
  }

  function clickAddButton(dialog) {
    assertTrue(!!dialog, 'Dialog is null for add');
    const addButton = dialog.$$('.action-button');
    assertTrue(!!addButton, 'Button is null');
    addButton.click();
  }

  function clickCancelButton(dialog) {
    assertTrue(!!dialog, 'Dialog is null for cancel');
    const cancelButton = dialog.$$('.cancel-button');
    assertTrue(!!cancelButton, 'Button is null');
    cancelButton.click();
  }

  function canAddPrinter(dialog, name, address) {
    dialog.newPrinter.printerName = name;
    dialog.newPrinter.printerAddress = address;
    return dialog.canAddPrinter_();
  }

  let page = null;
  let dialog = null;

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;

  setup(function() {
    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();
    settings.CupsPrintersBrowserProxyImpl.instance_ = cupsPrintersBrowserProxy;

    PolymerTest.clearBody();
    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);
    dialog = page.$$('settings-cups-add-printer-dialog');
    assertTrue(!!dialog);

    dialog.open();
    Polymer.dom.flush();
  });

  teardown(function() {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    dialog = null;
    page = null;
  });

  /**
   * Test that the discovery dialog is showing when a user initially asks
   * to add a printer.
   */
  test('DiscoveryShowing', function() {
    return PolymerTest.flushTasks().then(function() {
      // Discovery is showing.
      assertTrue(dialog.showDiscoveryDialog_);
      assertTrue(!!dialog.$$('add-printer-discovery-dialog'));

      // All other components are hidden.
      assertFalse(dialog.showManufacturerDialog_);
      assertFalse(!!dialog.$$('add-printer-manufacturer-model-dialog'));
      assertFalse(dialog.showConfiguringDialog_);
      assertFalse(!!dialog.$$('add-printer-configuring-dialog'));
      assertFalse(dialog.showManuallyAddDialog_);
      assertFalse(!!dialog.$$('add-printer-manually-dialog'));
    });
  });

  test('ValidIPV4', function() {
    const dialog = document.createElement('add-printer-manually-dialog');
    expectTrue(canAddPrinter(dialog, 'Test printer', '127.0.0.1'));
  });

  test('ValidIPV4WithPort', function() {
    const dialog = document.createElement('add-printer-manually-dialog');

    expectTrue(canAddPrinter(dialog, 'Test printer', '127.0.0.1:1234'));
  });

  test('ValidIPV6', function() {
    const dialog = document.createElement('add-printer-manually-dialog');

    // Test the full ipv6 address scheme.
    expectTrue(canAddPrinter(dialog, 'Test printer', '1:2:a3:ff4:5:6:7:8'));

    // Test the shorthand prefix scheme.
    expectTrue(canAddPrinter(dialog, 'Test printer', '::255'));

    // Test the shorthand suffix scheme.
    expectTrue(canAddPrinter(dialog, 'Test printer', '1::'));
  });

  test('ValidIPV6WithPort', function() {
    const dialog = document.createElement('add-printer-manually-dialog');

    expectTrue(canAddPrinter(dialog, 'Test printer', '[1:2:aa2:4]:12'));
    expectTrue(canAddPrinter(dialog, 'Test printer', '[::255]:54'));
    expectTrue(canAddPrinter(dialog, 'Test printer', '[1::]:7899'));
  });

  test('InvalidIPV6', function() {
    const dialog = document.createElement('add-printer-manually-dialog');

    expectFalse(canAddPrinter(dialog, 'Test printer', '1:2:3:4:5:6:7:8:9'));
    expectFalse(canAddPrinter(dialog, 'Test printer', '1:2:3:aa:a1245:2'));
    expectFalse(canAddPrinter(dialog, 'Test printer', '1:2:3:za:2'));
    expectFalse(canAddPrinter(dialog, 'Test printer', '1:::22'));
    expectFalse(canAddPrinter(dialog, 'Test printer', '1::2::3'));
  });

  test('ValidHostname', function() {
    const dialog = document.createElement('add-printer-manually-dialog');

    expectTrue(canAddPrinter(dialog, 'Test printer', 'hello-world.com'));
    expectTrue(canAddPrinter(dialog, 'Test printer', 'hello.world.com:12345'));
  });

  test('InvalidHostname', function() {
    const dialog = document.createElement('add-printer-manually-dialog');

    expectFalse(canAddPrinter(dialog, 'Test printer', 'helloworld!123.com'));
    expectFalse(canAddPrinter(dialog, 'Test printer', 'helloworld123-.com'));
    expectFalse(canAddPrinter(dialog, 'Test printer', '-helloworld123.com'));
  });

  /**
   * Test that clicking on Add opens the model select page.
   */
  test('ValidAddOpensModelSelection', function() {
    // Starts in discovery dialog, select add manually button.
    const discoveryDialog = dialog.$$('add-printer-discovery-dialog');
    assertTrue(!!discoveryDialog);
    discoveryDialog.$$('.secondary-button').click();
    Polymer.dom.flush();

    // Now we should be in the manually add dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    fillAddManuallyDialog(addDialog);

    addDialog.$$('.action-button').click();
    Polymer.dom.flush();
    // Configure is shown until getPrinterInfo is rejected.
    assertTrue(!!dialog.$$('add-printer-configuring-dialog'));

    // Upon rejection, show model.
    return cupsPrintersBrowserProxy
        .whenCalled('getCupsPrinterManufacturersList')
        .then(function() {
          return PolymerTest.flushTasks();
        })
        .then(function() {
          // Showing model selection.
          assertFalse(!!dialog.$$('add-printer-configuring-dialog'));
          assertTrue(!!dialog.$$('add-printer-manufacturer-model-dialog'));

          assertTrue(dialog.showManufacturerDialog_);
          assertFalse(dialog.showConfiguringDialog_);
          assertFalse(dialog.showManuallyAddDialog_);
          assertFalse(dialog.showDiscoveryDialog_);
        });
  });

  /**
   * Test that getModels isn't called with a blank query.
   */
  test('NoBlankQueries', function() {
    const discoveryDialog = dialog.$$('add-printer-discovery-dialog');
    assertTrue(!!discoveryDialog);
    discoveryDialog.$$('.secondary-button').click();
    Polymer.dom.flush();

    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    fillAddManuallyDialog(addDialog);

    // Verify that getCupsPrinterModelList is not called.
    cupsPrintersBrowserProxy.whenCalled('getCupsPrinterModelsList')
        .then(function(manufacturer) {
          assertNotReached(
              'No manufacturer was selected.  Unexpected model request.');
        });

    cupsPrintersBrowserProxy.manufacturers =
        ['ManufacturerA', 'ManufacturerB', 'Chromites'];
    addDialog.$$('.action-button').click();
    Polymer.dom.flush();

    return cupsPrintersBrowserProxy
        .whenCalled('getCupsPrinterManufacturersList')
        .then(function() {
          let modelDialog = dialog.$$('add-printer-manufacturer-model-dialog');
          assertTrue(!!modelDialog);
          // Manufacturer dialog has been rendered and the model list was not
          // requested.  We're done.
        });
  });

  /**
   * Test that dialog cancellation is logged from the manufacturer screen for
   * IPP printers.
   */
  test('LogDialogCancelledIpp', function() {
    const makeAndModel = 'Printer Make And Model';
    // Start on add manually.
    dialog.fire('open-manually-add-printer-dialog');
    Polymer.dom.flush();

    // Populate the printer object.
    dialog.newPrinter = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: '192.168.1.13',
      printerAutoconf: false,
      printerDescription: '',
      printerId: '',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipps',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };

    // Seed the getPrinterInfo response.  We detect the make and model but it is
    // not an autoconf printer.
    cupsPrintersBrowserProxy.printerInfo = {
      autoconf: false,
      manufacturer: 'MFG',
      model: 'MDL',
      makeAndModel: makeAndModel,
    };

    // Press the add button to advance dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    clickAddButton(addDialog);

    // Click cancel on the manufacturer dialog when it shows up then verify
    // cancel was called with the appropriate parameters.
    return cupsPrintersBrowserProxy
        .whenCalled('getCupsPrinterManufacturersList')
        .then(function() {
          Polymer.dom.flush();
          // Cancel setup with the cancel button.
          clickCancelButton(dialog.$$('add-printer-manufacturer-model-dialog'));
          return cupsPrintersBrowserProxy.whenCalled('cancelPrinterSetUp');
        })
        .then(function(printer) {
          assertTrue(!!printer, 'New printer is null');
          assertEquals(makeAndModel, printer.printerMakeAndModel);
        });
  });

  /**
   * Test that dialog cancellation is logged from the manufacturer screen for
   * USB printers.
   */
  test('LogDialogCancelledUSB', function() {
    const vendorId = 0x1234;
    const modelId = 0xDEAD;
    const manufacturer = 'PrinterMFG';
    const model = 'Printy Printerson';

    const usbInfo = {
      usbVendorId: vendorId,
      usbProductId: modelId,
      usbVendorName: manufacturer,
      usbProductName: model,
    };

    const expectedPrinter = 'PICK_ME!';

    const newPrinter = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: 'EEAADDAA',
      printerAutoconf: false,
      printerDescription: '',
      printerId: expectedPrinter,
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'printer',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'usb',
      printerQueue: 'moreinfohere',
      printerStatus: '',
      printerUsbInfo: usbInfo,
    };

    dialog.fire('open-discovery-printers-dialog');

    return cupsPrintersBrowserProxy.whenCalled('startDiscoveringPrinters')
        .then(function() {
          // Select the printer.
          // TODO(skau): Figure out how to select in a dom-repeat.
          let discoveryDialog = dialog.$$('add-printer-discovery-dialog');
          assertTrue(!!discoveryDialog, 'Cannot find discovery dialog');
          discoveryDialog.selectedPrinter = newPrinter;
          // Run printer setup.
          clickAddButton(discoveryDialog);
          return cupsPrintersBrowserProxy.whenCalled('addDiscoveredPrinter');
        })
        .then(function(printerId) {
          assertEquals(expectedPrinter, printerId);

          cr.webUIListenerCallback(
              'on-manually-add-discovered-printer', newPrinter);
          return cupsPrintersBrowserProxy.whenCalled(
              'getCupsPrinterManufacturersList');
        })
        .then(function() {
          // Cancel setup with the cancel button.
          clickCancelButton(dialog.$$('add-printer-manufacturer-model-dialog'));
          return cupsPrintersBrowserProxy.whenCalled('cancelPrinterSetUp');
        })
        .then(function(printer) {
          assertEquals(expectedPrinter, printer.printerId);
          assertDeepEquals(usbInfo, printer.printerUsbInfo);
        });
  });

  /**
   * Test that the close button exists on the configure dialog.
   */
  test('ConfigureDialogCancelDisabled', function() {
    // Starts in discovery dialog, select add manually button.
    const discoveryDialog = dialog.$$('add-printer-discovery-dialog');
    assertTrue(!!discoveryDialog);
    discoveryDialog.$$('.secondary-button').click();
    Polymer.dom.flush();

    // Now we should be in the manually add dialog.
    const addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    fillAddManuallyDialog(addDialog);

    // Advance to the configure dialog.
    addDialog.$$('.action-button').click();
    Polymer.dom.flush();

    // Configure is shown.
    const configureDialog = dialog.$$('add-printer-configuring-dialog');
    assertTrue(!!configureDialog);

    const closeButton = configureDialog.$$('.cancel-button');
    assertTrue(!!closeButton);
    assertFalse(closeButton.disabled);

    const waitForClose = test_util.eventToPromise('close', configureDialog);

    closeButton.click();
    Polymer.dom.flush();

    return waitForClose.then(() => {
      dialog = page.$$('settings-cups-add-printer-dialog');
      assertFalse(dialog.showConfiguringDialog_);
    });
  });
});

suite('EditPrinterDialog', function() {
  // Sets ppdManufacturer and ppdModel since ppdManufacturer has an observer
  // that erases ppdModel when ppdManufacturer changes.
  function setPpdManufacturerAndPpdModel(manufacturer, model) {
    dialog.activePrinter.ppdManufacturer = manufacturer;
    dialog.activePrinter.ppdModel = model;
  }

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;

  let dialog = null;

  setup(function() {
    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();
    settings.CupsPrintersBrowserProxyImpl.instance_ = cupsPrintersBrowserProxy;
    PolymerTest.clearBody();

    dialog = document.createElement('settings-cups-edit-printer-dialog');

    dialog.activePrinter = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: '',
      printerAutoconf: false,
      printerDescription: '',
      printerId: '',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: '',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: '',
      printerQueue: '',
      printerStatus: '',
    };

    document.body.appendChild(dialog);
  });

  teardown(function() {
    dialog.remove();
    dialog = null;
  });

  /**
   * Test that USB printers can be editted.
   */
  test('USBPrinterCanBeEdited', function() {
    dialog.activePrinter = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: '03f0/e414?serial=CD4234',
      printerAutoconf: false,
      printerDescription: '',
      printerId: '',
      printerManufacturer: '',
      printerModel: '',
      printerMakeAndModel: '',
      printerName: 'Test Printer',
      printerPPDPath: 'http://myfakeppddownload.com',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'usb',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };

    // Set activePrinter.ppdManufactuer and activePrinter.ppdModel to simulate
    // a printer for which we have a PPD.
    setPpdManufacturerAndPpdModel('manufacturer', 'model');

    // Edit the printer name.
    const nameField = dialog.$$('.printer-name-input');
    assertTrue(!!nameField);
    nameField.value = 'edited printer';

    // Assert that the "Save" button is enabled.
    const saveButton = dialog.$$('.action-button');
    assertTrue(!!saveButton);
    assertTrue(!saveButton.disabled);
  });

  /**
   * Test that the save button is disabled when the printer address or name is
   * invalid.
   */
  test('EditPrinter', function() {
    dialog.activePrinter = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: '192.168.1.13',
      printerAutoconf: false,
      printerDescription: '',
      printerId: '',
      printerManufacturer: '',
      printerModel: 'HP',
      printerMakeAndModel: 'Printmaster2000',
      printerName: 'My Test Printer',
      printerPPDPath: 'http://myfakeppddownload.com',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipps',
      printerQueue: 'moreinfohere',
      printerStatus: '',
    };

    assertTrue(!!dialog.$$('#printerName'));
    assertTrue(!!dialog.$$('#printerAddress'));

    const saveButton = dialog.$$('.action-button');
    assertTrue(!!saveButton);
    assertFalse(saveButton.disabled);

    // Change printer address to something invalid.
    dialog.$.printerAddress.value = 'abcdef:';
    assertTrue(saveButton.disabled);

    // Change back to something valid.
    dialog.$.printerAddress.value = 'abcdef:1234';
    assertFalse(saveButton.disabled);

    // Change printer name to empty
    dialog.$.printerName.value = '';
    assertTrue(saveButton.disabled);
  });
});
