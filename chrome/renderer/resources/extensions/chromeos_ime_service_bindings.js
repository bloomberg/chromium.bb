// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

if ((typeof mojo === 'undefined') || !mojo.bindingsLibraryInitialized) {
  loadScript('mojo_bindings');
}
mojo.config.autoLoadMojomDeps = false;

loadScript('chromeos.ime.mojom.input_engine.mojom');

/*
 * Represents the js-side of the InputChannel.
 * Routes calls from IME service to the IME extension.
 * @implements {chromeos.ime.mojom.InputChannel}
 */
class ImeExtensionChannel {
  constructor() {
    /**
     * @private @const
     * @type {!mojo.Binding}
     * */
    this.binding_ = new mojo.Binding(chromeos.ime.mojom.InputChannel, this);

    /**
     * @private
     * @type {chromeos.ime.mojom.InputChannelPtr}
     */
    this.channelPtr_ = undefined;

    /* Default result for calls without response. */
    this.emptyResult_ = Promise.resolve({result: ""});

    /**
     * Handler for the text message.
     *
     * @private
     * @type {function(string):string}
     */
    this.textHandler_ = undefined;

    /**
     * Handler for the protobuf message.
     *
     * @private
     * @type {function(Uint8Array):Uint8Array}
     */
    this.protobufHandler_ = undefined;
  }

  /**
   * Get a cached bound InterfacePtr for this InputChannel impl.
   * Create one the ptr if it's not bound yet.
   * @return {!chromeos.ime.mojom.InputChannelPtr}.
   */
  getChannelPtr() {
    return this.binding_.createInterfacePtrAndBind()
  }

  /**
   * Set a handler for processing text message. The handler must return a
   * nonnull string, otherwise it will lead to disconnection.
   * @param {function(string):string} handler.
   */
  onTextMessage(handler) {
    if (typeof handler == 'function') {
      this.textHandler_ = handler;
    }
    return this;
  }

  /**
   * Set a handler for processing protobuf message. The handler must return a
   * nonnull Uint8Array, otherwise it will lead to disconnection.
   * @param {function(!Uint8Array):!Uint8Array} handler.
   */
  onProtobufMessage(handler) {
    if (typeof handler == 'function') {
      this.protobufHandler_ = handler;
    }
    return this;
  }

  /**
   * Process the text message from a connected input engine.
   *
   * @private
   * @param {string} message
   * @return {!Promise<string>} result.
   */
  processText(message) {
    if (this.textHandler_) {
      return Promise.resolve({result: this.textHandler_(message)});
    }
    return this.emptyResult_;
  }

  /**
   * Process the protobuf message from a connected input engine.
   *
   * @private
   * @type {function(Uint8Array):Promise<Uint8Array>}
   * @return {!Promise<!Uint8Array>}
   */
  processMessage(message) {
    if (this.protobufHandler_) {
      return Promise.resolve({result: this.protobufHandler_(message)});
    }
    return this.emptyResult_;
  }

  /**
   * Set the error handler when the Mojo pipe is disconnected.
   * @param {function} handler.
   */
  onConnectionError(handler) {
    if (typeof handler == 'function') {
      this.binding_.setConnectionErrorHandler(handler);
    }
    return this;
  }
}

/*
 * The main entry point to the IME Mojo service.
 */
class ImeService {
  /** @param {!chromeos.ime.mojom.InputEngineManagerPtr} */
  constructor(manager) {
    /**
     * The IME Mojo service. Allows extension code to fetch an engine instance
     * implemented in the connected IME service.
     * @private
     * @type {!chromeos.ime.mojom.InputEngineManagerPtr}
     */
    this.manager_ = manager;

    /**
     * Handle to a KeepAlive service object, which prevents the extension from
     * being suspended as long as it remains in scope.
     * @private
     * @type {boolean}
     */
    this.keepAlive_ = null;

    /**
     * Current active IME Engine proxy. Allows extension code to make calls on
     * the connected InputEngine that resides in the IME service.
     * @private
     * @type {!chromeos.ime.mojom.InputChannelPtr}
     */
    this.activeEngine_ = null;

    /**
     * A to-client channel instance to receive data from the connected Engine
     * that resides in the IME service.
     * @private
     * @type {ImeExtensionChannel}
     */
    this.clientChannel_ = null;
  }

  /** @return {boolean} True if there is a connected IME service. */
  isConnected() {
    return this.manager_ && this.manager_.ptr.isBound();
  }

  /** @return {boolean} True if there is connected active IME engine. */
  isEngineActive() {
    return this.activeEngine_ && this.activeEngine_.ptr.isBound();
  }

  /**
   * Activates an input method based on its specification.
   * @param {string} The specification of an IME (e.g. the engine ID).
   * @param {?Uint8Array} The extra data (e.g. initial tasks to run).
   * @param {function(Object):void} The callback function to invoke when
   *     the IME activation is done.
   */
  activateIME(imeSpec, extra, callback) {
    if (this.isConnected()) {

      // Disconnect the current active engine and make a new one.
      // TODO(crbug.com/837156): Try to reuse the current engine if possible.
      if (this.isEngineActive()) {
        this.activeEngine_.ptr.reset();
        this.activeEngine_ = null;
      }
      this.activeEngine_ = new chromeos.ime.mojom.InputChannelPtr;

      // Create a client side channel to receive data from service.
      if (!this.clientChannel_) {
        this.clientChannel_ = new ImeExtensionChannel();
      }

      this.manager_.connectToImeEngine(
          imeSpec,
          mojo.makeRequest(this.activeEngine_),
          this.clientChannel_.getChannelPtr(),
          extra).then(callback);
    }
  }
}

(function() {
  let ptr = new chromeos.ime.mojom.InputEngineManagerPtr;
  Mojo.bindInterface(chromeos.ime.mojom.InputEngineManager.name,
                     mojo.makeRequest(ptr).handle);
  exports.$set('returnValue', new ImeService(ptr));
})();
