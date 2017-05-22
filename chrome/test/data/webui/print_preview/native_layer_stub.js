// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  /**
  * Test version of the native layer.
  * @constructor
  * @extends {settings.TestBrowserProxy}
  */
  function NativeLayerStub() {
    settings.TestBrowserProxy.call(this, [
        'getInitialSettings', 'setupPrinter' ]);

    /**
     * @private {!cr.EventTarget} The event target used for dispatching and
     *     receiving events.
     */
    this.eventTarget_ = new cr.EventTarget();

    /** @private {boolean} Whether the native layer has sent a print message. */
    this.printStarted_ = false;

    /**
     * @private {boolean} Whether the native layer has set the generate draft
     *      parameter when requesting an updated preview.
     */
    this.generateDraft_ = false;

    /**
     * @private {!print_preview.NativeInitialSettings} The initial settings
     *     to be used for the response to a |getInitialSettings| call.
     */
    this.initialSettings_ = null;

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
     * @private {string} The destination id to watch for counting calls to
     *     |getLocalDestinationCapabilities|.
     */
    this.destinationToWatch_ = '';

    /**
     * @private {number} The number of calls to
     *     |getLocalDestinationCapabilities| with id = |destinationToWatch_|.
     */
    this.getLocalDestinationCapabilitiesCallCount_ = 0;
  }

  NativeLayerStub.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    getInitialSettings: function() {
      this.methodCalled('getInitialSettings');
      return Promise.resolve(this.initialSettings_);
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
    startGetLocalDestinations: function() {},
    startGetPrivetDestinations: function() {},
    startGetExtensionDestinations: function() {},
    startGetLocalDestinationCapabilities: function(destinationId) {
      if (destinationId == this.destinationToWatch_)
        this.getLocalDestinationCapabilitiesCallCount_++;
    },
    startGetPreview: function(destination, printTicketStore, documentInfo,
                              generateDraft, requestId) {
      this.generateDraft_ = generateDraft;
    },
    startPrint: function () { this.printStarted_ = true; },
    startHideDialog: function () {},

    /** @return {!cr.EventTarget} The native layer event target. */
    getEventTarget: function() { return this.eventTarget_; },

    /** @param {!cr.EventTarget} eventTarget The event target to use. */
    setEventTarget: function(eventTarget) {
      this.eventTarget_ = eventTarget;
    },

    /**
     * @return {boolean} Whether capabilities have been requested exactly once
     *     for |destinationToWatch_|.
     */
    didGetCapabilitiesOnce: function(destinationId) {
      return (destinationId == this.destinationToWatch_ &&
              this.getLocalDestinationCapabilitiesCallCount_ == 1);
    },

    /** @return {boolean} Whether a new draft was requested for preview. */
    generateDraft: function() { return this.generateDraft_; },

    /** @return {boolean} Whether a print request has been issued. */
    isPrintStarted: function() { return this.printStarted_; },

    /**
     * @param {string} destinationId The destination ID to watch for
     *     |getLocalDestinationCapabilities| calls.
     * Resets |getLocalDestinationCapabilitiesCallCount_|.
     */
    setDestinationToWatch: function(destinationId) {
      this.destinationToWatch_ = destinationId;
      this.getLocalDestinationCapabilitiesCallCount_ = 0;
    },

    /**
     * @param {!print_preview.NativeInitialSettings} settings The settings
     *     to return as a response to |getInitialSettings|.
     */
    setInitialSettings: function(settings) {
      this.initialSettings_ = settings;
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
  };

  return {
    NativeLayerStub: NativeLayerStub,
  };
});
