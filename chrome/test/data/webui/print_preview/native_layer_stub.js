// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  /**
  * Test version of the native layer.
  * @constructor
  * @extends {TestBrowserProxy}
  */
  function NativeLayerStub() {
    TestBrowserProxy.call(this, [
        'getInitialSettings',
        'getPrinters',
        'getExtensionPrinters',
        'getPreview',
        'getPrivetPrinters',
        'getPrinterCapabilities',
        'print',
        'setupPrinter'
      ]);

    /**
     * @private {!cr.EventTarget} The event target used for dispatching and
     *     receiving events.
     */
    this.eventTarget_ = new cr.EventTarget();

    /**
     * @private {!print_preview.NativeInitialSettings} The initial settings
     *     to be used for the response to a |getInitialSettings| call.
     */
    this.initialSettings_ = null;

    /**
     *
     * @private {!Array<!print_preview.LocalDestinationInfo>} Local destination
     *     list to be used for the response to |getPrinters|.
     */
    this.localDestinationInfos_ = [];

    /**
     * @private {!Map<string,
     *                !Promise<!print_preview.PrinterCapabilitiesResponse>}
     *     A map from destination IDs to the responses to be sent when
     *     |getPrinterCapabilities| is called for the ID.
     */
    this.localDestinationCapabilities_ = new Map();

    /**
     * @private {!print_preview.PrinterSetupResponse} The response to be sent
     *     on a |setupPrinter| call.
     */
    this.setupPrinterResponse_ = null;

    /**
     * @private {boolean} Whether the printer setup request should be rejected.
     */
    this.shouldRejectPrinterSetup_ = false;

    /**
     * @private {string} The ID of a printer with a bad driver.
     */
    this.badPrinterId_ = '';
  }

  NativeLayerStub.prototype = {
    __proto__: TestBrowserProxy.prototype,

    /** @override */
    getInitialSettings: function() {
      this.methodCalled('getInitialSettings');
      return Promise.resolve(this.initialSettings_);
    },

    /** @override */
    getPrinters: function() {
      this.methodCalled('getPrinters');
      return Promise.resolve(this.localDestinationInfos_);
    },

    /** @override */
    getExtensionPrinters: function() {
      this.methodCalled('getExtensionPrinters');
      return Promise.resolve(true);
    },

    /** @override */
    getPreview: function(
        destination, printTicketStore, documentInfo, generateDraft, requestId) {
      this.methodCalled('getPreview', {
        destination: destination,
        printTicketStore: printTicketStore,
        documentInfo: documentInfo,
        generateDraft: generateDraft,
        requestId: requestId,
      });
      var rejectString = print_preview.PreviewArea.EventType.SETTINGS_INVALID;
      rejectString = rejectString.substring(
          rejectString.lastIndexOf(".") + 1, rejectString.length);
      return destination.id == this.badPrinterId_ ?
          Promise.reject(rejectString) :
          Promise.resolve(requestId);
    },

    /** @override */
    getPrivetPrinters: function() {
      this.methodCalled('getPrivetPrinters');
      return Promise.resolve(true);
    },

    /** @override */
    getPrinterCapabilities: function(printerId) {
      this.methodCalled('getPrinterCapabilities', printerId);
      return this.localDestinationCapabilities_.get(printerId);
    },

    /** @override */
    print: function(
        destination, printTicketStore, cloudPrintInterface, documentInfo) {
      this.methodCalled('print', {
        destination: destination,
        printTicketStore: printTicketStore,
        cloudPrintInterface: cloudPrintInterface,
        documentInfo: documentInfo,
      });
      return Promise.resolve();
    },

    /** @override */
    setupPrinter: function(printerId) {
      this.methodCalled('setupPrinter', printerId);
      return this.shouldRejectPrinterSetup_ ?
          Promise.reject(this.setupPrinterResponse_) :
          Promise.resolve(this.setupPrinterResponse_);
    },

    /** Stubs for |print_preview.NativeLayer| methods that call C++ handlers. */
    previewReadyForTest: function() {},
    startHideDialog: function () {},

    /** @return {!cr.EventTarget} The native layer event target. */
    getEventTarget: function() { return this.eventTarget_; },

    /** @param {!cr.EventTarget} eventTarget The event target to use. */
    setEventTarget: function(eventTarget) {
      this.eventTarget_ = eventTarget;
    },

    /**
     * @param {!print_preview.NativeInitialSettings} settings The settings
     *     to return as a response to |getInitialSettings|.
     */
    setInitialSettings: function(settings) {
      this.initialSettings_ = settings;
    },

    /**
     * @param {!Array<!print_preview.LocalDestinationInfo>} localDestinations
     *     The local destinations to return as a response to |getPrinters|.
     */
    setLocalDestinations: function(localDestinations) {
      this.localDestinationInfos_ = localDestinations;
    },

    /**
     * @param {!print_preview.PrinterCapabilitiesResponse} response The
     *     response to send for the destination whose ID is in the response.
     * @param {boolean?} opt_reject Whether to reject the callback for this
     *     destination. Defaults to false (will resolve callback) if not
     *     provided.
     */
    setLocalDestinationCapabilities: function(response, opt_reject) {
      this.localDestinationCapabilities_.set(response.printerId,
          opt_reject ? Promise.reject() : Promise.resolve(response));
    },

    /**
     * @param {boolean} reject Whether printSetup requests should be rejected.
     * @param {!print_preview.PrinterSetupResponse} The response to send when
     *     |setupPrinter| is called.
     */
    setSetupPrinterResponse: function(reject, response) {
      this.shouldRejectPrinterSetup_ = reject;
      this.setupPrinterResponse_ = response;
    },

    /**
     * @param {string} bad_id The printer ID that should cause an
     *     SETTINGS_INVALID error in response to a preview request. Models a
     *     bad printer driver.
     */
    setInvalidPrinterId: function(id) {
      this.badPrinterId_ = id;
    },
  };

  return {
    NativeLayerStub: NativeLayerStub,
  };
});
