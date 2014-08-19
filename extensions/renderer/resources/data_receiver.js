// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('data_receiver', [
    'async_waiter',
    'device/serial/data_stream.mojom',
    'mojo/public/js/bindings/core',
    'mojo/public/js/bindings/router',
], function(asyncWaiter, dataStream, core, router) {
  /**
   * @module data_receiver
   */

  /**
   * @typedef module:data_receiver~PendingError
   * @type {Object}
   * @property {number} error - the error
   * @property {number} offset - the location of the error
   * @private
   */

  /**
   * A pending receive operation.
   * @constructor
   * @alias module:data_receiver~PendingReceive
   * @private
   */
  function PendingReceive() {
    /**
     * The promise that will be resolved or rejected when this receive completes
     * or fails, respectively.
     * @type {Promise.<ArrayBuffer>}
     * @private
     */
    this.promise_ = new Promise(function(resolve, reject) {
      /**
       * The callback to call with the data received on success.
       * @type {Function}
       * @private
       */
      this.dataCallback_ = resolve;
      /**
       * The callback to call with the error on failure.
       * @type {Function}
       * @private
       */
      this.errorCallback_ = reject;
    }.bind(this));
  }

  /**
   * Returns the promise that will be resolved when this operation completes or
   * rejected if an error occurs.
   * @return {Promise.<ArrayBuffer>} A promise to the data received.
   */
  PendingReceive.prototype.getPromise = function() {
    return this.promise_;
  };

  /**
   * Dispatches received data to the promise returned by
   * [getPromise]{@link module:data_receiver.PendingReceive#getPromise}.
   * @param {ArrayBuffer} data The data to dispatch.
   */
  PendingReceive.prototype.dispatchData = function(data) {
    this.dataCallback_(data);
  };

  /**
   * Dispatches an error if the offset of the error has been reached.
   * @param {module:data_receiver~PendingError} error The error to dispatch.
   * @param {number} bytesReceived The number of bytes that have been received.
   */
  PendingReceive.prototype.dispatchError = function(error, bytesReceived) {
    if (bytesReceived != error.offset)
      return false;

    var e = new Error();
    e.error = error.error;
    this.errorCallback_(e);
    return true;
  };

  /**
   * Unconditionally dispatches an error.
   * @param {number} error The error to dispatch.
   */
  PendingReceive.prototype.dispatchFatalError = function(error) {
    var e = new Error();
    e.error = error;
    this.errorCallback_(e);
  };

  /**
   * A DataReceiver that receives data from a DataSource.
   * @param {MojoHandle} handle The handle to the DataSource.
   * @param {number} bufferSize How large a buffer the data pipe should use.
   * @param {number} fatalErrorValue The receive error value to report in the
   *     event of a fatal error.
   * @constructor
   * @alias module:data_receiver.DataReceiver
   */
  function DataReceiver(handle, bufferSize, fatalErrorValue) {
    /**
     * The [Router]{@link module:mojo/public/js/bindings/router.Router} for the
     * connection to the DataSource.
     * @private
     */
    this.router_ = new router.Router(handle);
    /**
     * The connection to the DataSource.
     * @private
     */
    this.source_ = new dataStream.DataSourceProxy(this.router_);
    this.router_.setIncomingReceiver(this);
    var dataPipeOptions = {
      flags: core.CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,
      elementNumBytes: 1,
      capacityNumBytes: bufferSize,
    };
    var receivePipe = core.createDataPipe(dataPipeOptions);
    this.source_.init(receivePipe.producerHandle);
    /**
     * The handle to the data pipe to use for receiving data.
     * @private
     */
    this.receivePipe_ = receivePipe.consumerHandle;
    /**
     * The current receive operation.
     * @type {module:data_receiver~PendingReceive}
     * @private
     */
    this.receive_ = null;
    /**
     * The error to be dispatched in the event of a fatal error.
     * @type {number}
     * @private
     */
    this.fatalErrorValue_ = fatalErrorValue;
    /**
     * The async waiter used to wait for
     * {@link module:data_receiver.DataReceiver#receivePipe_} to be readable.
     * @type module:async_waiter.AsyncWaiter
     * @private
     */
    this.waiter_ = new asyncWaiter.AsyncWaiter(this.receivePipe_,
                                               core.HANDLE_SIGNAL_READABLE,
                                               this.onHandleReady_.bind(this));
    /**
     * The number of bytes received from the DataSource.
     * @type {number}
     * @private
     */
    this.bytesReceived_ = 0;
    /**
     * The pending error if there is one.
     * @type module:data_receiver~PendingError
     * @private
     */
    this.pendingError_ = null;
    /**
     * Whether the DataSource is paused.
     * @type {boolean}
     * @private
     */
    this.paused_ = false;
    /**
     * Whether this DataReceiver has shut down.
     * @type {boolean}
     * @private
     */
    this.shutDown_ = false;
  }

  DataReceiver.prototype =
      $Object.create(dataStream.DataSourceClientStub.prototype);

  /**
   * Closes this DataReceiver.
   */
  DataReceiver.prototype.close = function() {
    if (this.shutDown_)
      return;
    this.shutDown_ = true;
    this.router_.close();
    this.waiter_.stop();
    core.close(this.receivePipe_);
    if (this.receive_) {
      this.receive_.dispatchFatalError(this.fatalErrorValue_);
      this.receive_ = null;
    }
  };

  /**
   * Receive data from the DataSource.
   * @return {Promise.<ArrayBuffer>} A promise to the received data. If an error
   *     occurs, the promise will reject with an Error object with a property
   *     error containing the error code.
   * @throws Will throw if this has encountered a fatal error or another receive
   *     is in progress.
   */
  DataReceiver.prototype.receive = function() {
    if (this.shutDown_)
      throw new Error('System error');
    if (this.receive_)
      throw new Error('Receive already in progress.');
    var receive = new PendingReceive();
    var promise = receive.getPromise();
    if (this.pendingError_ &&
        receive.dispatchError(this.pendingError_, this.bytesReceived_)) {
      this.pendingError_ = null;
      this.paused_ = true;
      return promise;
    }
    if (this.paused_) {
      this.source_.resume();
      this.paused_ = false;
    }
    this.receive_ = receive;
    this.waiter_.start();
    return promise;
  };

  /**
   * Invoked when |handle_| is ready to read. Reads from the data pipe if the
   * wait is successful.
   * @param {number} waitResult The result of the asynchronous wait.
   * @private
   */
  DataReceiver.prototype.onHandleReady_ = function(waitResult) {
    if (waitResult != core.RESULT_OK || !this.receive_) {
      this.close();
      return;
    }
    var result = core.readData(this.receivePipe_, core.READ_DATA_FLAG_NONE);
    if (result.result == core.RESULT_OK) {
      // TODO(sammc): Handle overflow in the same fashion as the C++ receiver.
      this.bytesReceived_ += result.buffer.byteLength;
      this.receive_.dispatchData(result.buffer);
      this.receive_ = null;
    } else if (result.result == core.RESULT_SHOULD_WAIT) {
      this.waiter_.start();
    } else {
      this.close();
    }
  };

  /**
   * Invoked by the DataSource when an error is encountered.
   * @param {number} offset The location at which the error occurred.
   * @param {number} error The error that occurred.
   * @private
   */
  DataReceiver.prototype.onError = function(offset, error) {
    if (this.shutDown_)
      return;

    /**
     * @type module:data_receiver~PendingError
     */
    var pendingError = {
      error: error,
      offset: offset,
    };
    if (this.receive_ &&
        this.receive_.dispatchError(pendingError, this.bytesReceived_)) {
      this.receive_ = null;
      this.waiter_.stop();
      this.paused_ = true;
      return;
    }
    this.pendingError_ = pendingError;
  };

  return {DataReceiver: DataReceiver};
});
