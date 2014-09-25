// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('data_receiver', [
    'async_waiter',
    'device/serial/data_stream.mojom',
    'device/serial/data_stream_serialization.mojom',
    'mojo/public/js/bindings/core',
    'mojo/public/js/bindings/router',
], function(asyncWaiter, dataStream, serialization, core, router) {
  /**
   * @module data_receiver
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
     * @type {!Promise.<ArrayBuffer>}
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
   * @param {!ArrayBuffer} data The data to dispatch.
   */
  PendingReceive.prototype.dispatchData = function(data) {
    this.dataCallback_(data);
  };

  /**
   * Dispatches an error if the offset of the error has been reached.
   * @param {!PendingReceiveError} error The error to dispatch.
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
   * @param {!MojoHandle} handle The handle to the DataSource.
   * @param {number} bufferSize How large a buffer the data pipe should use.
   * @param {number} fatalErrorValue The receive error value to report in the
   *     event of a fatal error.
   * @constructor
   * @alias module:data_receiver.DataReceiver
   */
  function DataReceiver(handle, bufferSize, fatalErrorValue) {
    var dataPipeOptions = {
      flags: core.CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,
      elementNumBytes: 1,
      capacityNumBytes: bufferSize,
    };
    var receivePipe = core.createDataPipe(dataPipeOptions);
    this.init_(
        handle, receivePipe.consumerHandle, fatalErrorValue, 0, null, false);
    this.source_.init(receivePipe.producerHandle);
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
   * Initialize this DataReceiver.
   * @param {!MojoHandle} source A handle to the DataSource
   * @param {!MojoHandle} dataPipe A handle to use for receiving data from the
   *     DataSource.
   * @param {number} fatalErrorValue The error to dispatch in the event of a
   *     fatal error.
   * @param {number} bytesReceived The number of bytes already received.
   * @param {PendingReceiveError} pendingError The pending error if there is
   * one.
   * @param {boolean} paused Whether the DataSource is paused.
   * @private
   */
  DataReceiver.prototype.init_ = function(source,
                                          dataPipe,
                                          fatalErrorValue,
                                          bytesReceived,
                                          pendingError,
                                          paused) {
    /**
     * The [Router]{@link module:mojo/public/js/bindings/router.Router} for the
     * connection to the DataSource.
     * @private
     */
    this.router_ = new router.Router(source);
    /**
     * The connection to the DataSource.
     * @private
     */
    this.source_ = new dataStream.DataSourceProxy(this.router_);
    this.router_.setIncomingReceiver(this);
    /**
     * The handle to the data pipe to use for receiving data.
     * @private
     */
    this.receivePipe_ = dataPipe;
    /**
     * The current receive operation.
     * @type {module:data_receiver~PendingReceive}
     * @private
     */
    this.receive_ = null;
    /**
     * The error to be dispatched in the event of a fatal error.
     * @const {number}
     * @private
     */
    this.fatalErrorValue_ = fatalErrorValue;
    /**
     * The async waiter used to wait for
     * |[receivePipe_]{@link module:data_receiver.DataReceiver#receivePipe_}| to
     *     be readable.
     * @type {!module:async_waiter.AsyncWaiter}
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
    this.bytesReceived_ = bytesReceived;
    /**
     * The pending error if there is one.
     * @type {PendingReceiveError}
     * @private
     */
    this.pendingError_ = pendingError;
    /**
     * Whether the DataSource is paused.
     * @type {boolean}
     * @private
     */
    this.paused_ = paused;
    /**
     * Whether this DataReceiver has shut down.
     * @type {boolean}
     * @private
     */
    this.shutDown_ = false;
  };

  /**
   * Serializes this DataReceiver.
   * This will cancel a receive if one is in progress.
   * @return {!Promise.<SerializedDataReceiver>} A promise that will resolve to
   *     the serialization of this DataReceiver. If this DataReceiver has shut
   *     down, the promise will resolve to null.
   */
  DataReceiver.prototype.serialize = function() {
    if (this.shutDown_)
      return Promise.resolve(null);

    this.waiter_.stop();
    if (this.receive_) {
      this.receive_.dispatchFatalError(this.fatalErrorValue_);
      this.receive_ = null;
    }
    var serialized = new serialization.SerializedDataReceiver();
    serialized.source = this.router_.connector_.handle_;
    serialized.data_pipe = this.receivePipe_;
    serialized.fatal_error_value = this.fatalErrorValue_;
    serialized.bytes_received = this.bytesReceived_;
    serialized.paused = this.paused_;
    serialized.pending_error = this.pendingError_;
    this.router_.connector_.handle_ = null;
    this.router_.close();
    this.shutDown_ = true;
    return Promise.resolve(serialized);
  };

  /**
   * Deserializes a SerializedDataReceiver.
   * @param {SerializedDataReceiver} serialized The serialized DataReceiver.
   * @return {!DataReceiver} The deserialized DataReceiver.
   */
  DataReceiver.deserialize = function(serialized) {
    var receiver = $Object.create(DataReceiver.prototype);
    receiver.deserialize_(serialized);
    return receiver;
  };

  /**
   * Deserializes a SerializedDataReceiver into this DataReceiver.
   * @param {SerializedDataReceiver} serialized The serialized DataReceiver.
   * @private
   */
  DataReceiver.prototype.deserialize_ = function(serialized) {
    if (!serialized) {
      this.shutDown_ = true;
      return;
    }
    this.init_(serialized.source,
               serialized.data_pipe,
               serialized.fatal_error_value,
               serialized.bytes_received,
               serialized.pending_error,
               serialized.paused);
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
      throw new Error('DataReceiver has been closed');
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
   * Invoked when
   * |[receivePipe_]{@link module:data_receiver.DataReceiver#receivePipe_}| is
   * ready to read. Reads from the data pipe if the wait is successful.
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

    var pendingError = new serialization.PendingReceiveError();
    pendingError.error = error;
    pendingError.offset = offset;
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
