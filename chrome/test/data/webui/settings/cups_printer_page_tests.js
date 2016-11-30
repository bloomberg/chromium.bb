// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @implements {settings.CupsPrintersBrowserProxy}
 * @extends {settings.TestBrowserProxy}
 */
var TestCupsPrintersBrowserProxy = function() {
  settings.TestBrowserProxy.call(this, [
    'getCupsPrintersList',
    'getCupsPrinterManufacturersList',
    'getCupsPrinterModelsList'
  ]);
};

TestCupsPrintersBrowserProxy.prototype = {
  __proto__: settings.TestBrowserProxy.prototype,

  printerList: [],
  manufacturers: [],
  models: [],

  /** @override */
  getCupsPrintersList: function() {
    this.methodCalled('getCupsPrintersList');
    return Promise.resolve(this.printerList);
  },

  /** @override */
  getCupsPrinterManufacturersList: function() {
    this.methodCalled('getCupsPrinterManufacturersList');
    return Promise.resolve(this.manufacturers);
  },

  /** @override */
  getCupsPrinterModelsList: function(manufacturer) {
    this.methodCalled('getCupsPrinterModelsList', manufacturer);
    return Promise.resolve(this.models);
  },
};

suite('CupsAddPrinterDialogTests', function() {
  function fillDialog(addDialog) {
    var name = addDialog.$$('#printerNameInput');
    var address = addDialog.$$('#printerAddressInput');

    assertTrue(!!name);
    name.value = 'Test Printer';

    assertTrue(!!address);
    address.value = '127.0.0.1';
  }

  var page = null;
  var dialog = null;

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  var cupsPrintersBrowserProxy = null;

  setup(function() {
    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();
    settings.CupsPrintersBrowserProxyImpl.instance_ = cupsPrintersBrowserProxy;

    cupsPrintersBrowserProxy.reset();

    PolymerTest.clearBody();
    page = document.createElement('settings-cups-printers');
    dialog = document.createElement('settings-cups-add-printer-dialog');
    document.body.appendChild(page);
    page.appendChild(dialog);

    dialog.open();
    Polymer.dom.flush();
  });

  teardown(function() {
    page.remove();
    dialog = null;
    page = null;
  });

  /**
   * Test that the manual add dialog is showing.
   */
  test('ManualAddShowing', function() {
    assertFalse(!!dialog.$$('add-printer-manufacturer-model-dialog'));
    assertFalse(!!dialog.$$('add-printer-configuring-dialog'));
    assertTrue(!!dialog.$$('add-printer-manually-dialog'));
  });

  /**
   * Test that clicking on Add opens the model select page.
   */
  test('ValidAddOpensModelSelection', function() {
    var addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);

    fillDialog(addDialog);

    MockInteractions.tap(addDialog.$$('.action-button'));
    Polymer.dom.flush();

    // showing model selection
    assertFalse(!!dialog.$$('add-printer-configuring-dialog'));
    assertTrue(!!dialog.$$('add-printer-manufacturer-model-dialog'));
  });

  /**
   * Test that getModels isn't called with a blank query.
   */
  test('NoBlankQueries', function() {
    var addDialog = dialog.$$('add-printer-manually-dialog');
    assertTrue(!!addDialog);
    fillDialog(addDialog);

    cupsPrintersBrowserProxy.whenCalled('getCupsPrinterModelsList')
        .then(function(manufacturer) { assertGT(0, manufacturer.length); });

    cupsPrintersBrowserProxy.manufacturers =
        ['ManufacturerA', 'ManufacturerB', 'Chromites'];
    MockInteractions.tap(addDialog.$$('.action-button'));
    Polymer.dom.flush();

    var modelDialog = dialog.$$('add-printer-manufacturer-model-dialog');
    assertTrue(!!modelDialog);

    return cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
  });
});
