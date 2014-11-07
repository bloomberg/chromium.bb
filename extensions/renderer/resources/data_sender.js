// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('data_sender', [
    'device/serial/data_stream.mojom',
    'device/serial/data_stream_serialization.mojom',
    'mojo/public/js/core',
    'mojo/public/js/router',
], function(dataStreamMojom, serialization, core, routerModule) {
  /**
   * @module data_sender
   */

  /**
   * A pending send operation.
   * @param {!ArrayBuffer} data The data to be sent.
   * @constructor
   * @alias module:data_sender~PendingSend
   * @private
   */
  function PendingSend(data) {
    /**
     * The remaining data to be sent.
     * @type {!ArrayBuffer}
     * @private
     */
    this.data_ = data;
    /**
     * The total length of data to be sent.
     * @type {number}
     * @private
     */
    this.length_ = data.byteLength;
    /**
     * The number of bytes that have been received by the DataSink.
     * @type {number}
     * @private
     */
    this.bytesReceivedBySink_ = 0;
    /**
     * The promise that will be resolved or rejected when this send completes
     * or fails, respectively.
     * @type {!Promise.<number>}
     * @private
     */
    this.promise_ = new Promise(function(resolve, reject) {
      /**
       * The callback to call on success.
       * @type {Function}
       * @private
       */
      this.successCallback_ = resolve;
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
   * @return {!Promise.<number>} A promise to the number of bytes sent.
   */
  PendingSend.prototype.getPromise = function() {
    return this.promise_;
  };

  /**
   * @typedef module:data_sender~PendingSend.ReportBytesResult
   * @property {number} bytesUnreported The number of bytes reported that were
   *     not part of the send.
   * @property {boolean} done Whether this send has completed.
   * @property {?number} bytesToFlush The number of bytes to flush in the event
   *     of an error.
   */

  /**
   * Invoked when the DataSink reports that bytes have been sent. Resolves the
   * promise returned by
   * [getPromise()]{@link module:data_sender~PendingSend#getPromise} once all
   * bytes have been reported as sent.
   * @param {number} numBytes The number of bytes sent.
   * @return {!module:data_sender~PendingSend.ReportBytesResult}
   */
  PendingSend.prototype.reportBytesSent = function(numBytes) {
    var result = this.reportBytesSentInternal_(numBytes);
    if (this.bytesReceivedBySink_ == this.length_) {
      result.done = true;
      this.successCallback_(this.bytesReceivedBySink_);
    }
    return result;
  };

  /**
   * Invoked when the DataSink reports an error. Rejects the promise returned by
   * [getPromise()]{@link module:data_sender~PendingSend#getPromise} unless the
   * error occurred after this send, that is, unless numBytes is greater than
   * the nubmer of outstanding bytes.
   * @param {number} numBytes The number of bytes sent.
   * @param {number} error The error reported by the DataSink.
   * @return {!module:data_sender~PendingSend.ReportBytesResult}
   */
  PendingSend.prototype.reportBytesSentAndError = function(numBytes, error) {
    var result = this.reportBytesSentInternal_(numBytes);
    // If there are remaining bytes to report, the error occurred after this
    // PendingSend so we should report success.
    if (result.bytesUnreported > 0) {
      this.successCallback_(this.bytesReceivedBySink_);
      result.bytesToFlush = 0;
      return result;
    }

    var e = new Error();
    e.error = error;
    e.bytesSent = this.bytesReceivedBySink_;
    this.errorCallback_(e);
    this.done = true;
    result.bytesToFlush =
        this.length_ - this.data_.byteLength - this.bytesReceivedBySink_;
    return result;
  };

  /**
   * Updates the internal state in response to a report from the DataSink.
   * @param {number} numBytes The number of bytes sent.
   * @return {!module:data_sender~PendingSend.ReportBytesResult}
   * @private
   */
  PendingSend.prototype.reportBytesSentInternal_ = function(numBytes) {
    this.bytesReceivedBySink_ += numBytes;
    var result = {bytesUnreported: 0};
    if (this.bytesReceivedBySink_ > this.length_) {
      result.bytesUnreported = this.bytesReceivedBySink_ - this.length_;
      this.bytesReceivedBySink_ = this.length_;
    }
    result.done = false;
    return result;
  };

  /**
   * Writes pending data into the data pipe.
   * @param {!DataSink} sink The DataSink to receive the data.
   * @param {number} availableBufferCapacity The maximum number of bytes to
   *     send.
   * @return {!Object} result The send result.
   * @return {boolean} result.completed Whether all of the pending data was
   *     sent.
   * @return {number} result.remainingBufferCapacity The remaining send buffer
   *     capacity.
   */
  PendingSend.prototype.sendData = function(sink, availableBufferCapacity) {
    var numBytesToSend =
        Math.min(availableBufferCapacity, this.data_.byteLength);
    sink.onData(new Uint8Array(this.data_, 0, numBytesToSend));
    this.data_ = this.data_.slice(numBytesToSend);
    return {
      completed: this.data_.byteLength == 0,
      remainingBufferCapacity: availableBufferCapacity - numBytesToSend,
    };
  };

  /**
   * A DataSender that sends data to a DataSink.
   * @param {!MojoHandle} handle The handle to the DataSink.
   * @param {number} bufferSize How large a buffer to use for data.
   * @param {number} fatalErrorValue The send error value to report in the
   *     event of a fatal error.
   * @constructor
   * @alias module:data_sender.DataSender
   */
  function DataSender(handle, bufferSize, fatalErrorValue) {
    this.init_(handle, fatalErrorValue, bufferSize);
    this.sink_.init(bufferSize);
  }

  DataSender.prototype =
      $Object.create(dataStreamMojom.DataSinkClient.stubClass.prototype);

  /**
   * Closes this DataSender.
   */
  DataSender.prototype.close = function() {
    if (this.shutDown_)
      return;
    this.shutDown_ = true;
    this.router_.close();
    while (this.pendingSends_.length) {
      this.pendingSends_.pop().reportBytesSentAndError(
          0, this.fatalErrorValue_);
    }
    while (this.sendsAwaitingAck_.length) {
      this.sendsAwaitingAck_.pop().reportBytesSentAndError(
          0, this.fatalErrorValue_);
    }
    this.callCancelCallback_();
  };

  /**
   * Initialize this DataSender.
   * @param {!MojoHandle} sink A handle to the DataSink
   * @param {number} fatalErrorValue The error to dispatch in the event of a
   *     fatal error.
   * @param {number} bufferSize The size of the send buffer.
   * @private
   */
  DataSender.prototype.init_ = function(sink, fatalErrorValue, bufferSize) {
    /**
     * The error to be dispatched in the event of a fatal error.
     * @const {number}
     * @private
     */
    this.fatalErrorValue_ = fatalErrorValue;
    /**
     * Whether this DataSender has shut down.
     * @type {boolean}
     * @private
     */
    this.shutDown_ = false;
    /**
     * The [Router]{@link module:mojo/public/js/router.Router} for the
     * connection to the DataSink.
     * @private
     */
    this.router_ = new routerModule.Router(sink);
    /**
     * The connection to the DataSink.
     * @private
     */
    this.sink_ = new dataStreamMojom.DataSink.proxyClass(this.router_);
    this.router_.setIncomingReceiver(this);
    /**
     * A queue of sends that have not fully sent their data to the DataSink.
     * @type {!module:data_sender~PendingSend[]}
     * @private
     */
    this.pendingSends_ = [];
    /**
     * A queue of sends that have sent their data to the DataSink, but have not
     * been received by the DataSink.
     * @type {!module:data_sender~PendingSend[]}
     * @private
     */
    this.sendsAwaitingAck_ = [];

    /**
     * The callback that will resolve a pending cancel if one is in progress.
     * @type {?Function}
     * @private
     */
    this.pendingCancel_ = null;

    /**
     * The promise that will be resolved when a pending cancel completes if one
     * is in progress.
     * @type {Promise}
     * @private
     */
    this.cancelPromise_ = null;
    /**
     * The available send buffer capacity.
     * @type {number}
     * @private
     */
    this.availableBufferCapacity_ = bufferSize;
  };

  /**
   * Serializes this DataSender.
   * This will cancel any sends in progress before the returned promise
   * resolves.
   * @return {!Promise.<SerializedDataSender>} A promise that will resolve to
   *     the serialization of this DataSender. If this DataSender has shut down,
   *     the promise will resolve to null.
   */
  DataSender.prototype.serialize = function() {
    if (this.shutDown_)
      return Promise.resolve(null);

    var readyToSerialize = Promise.resolve();
    if (this.pendingSends_.length || this.sendsAwaitingAck_.length) {
      if (this.pendingCancel_)
        readyToSerialize = this.cancelPromise_;
      else
        readyToSerialize = this.cancel(this.fatalErrorValue_);
    }
    return readyToSerialize.then(function() {
      var serialized = new serialization.SerializedDataSender();
      serialized.sink = this.router_.connector_.handle_;
      serialized.fatal_error_value = this.fatalErrorValue_;
      serialized.buffer_size = this.availableBufferCapacity_;
      this.router_.connector_.handle_ = null;
      this.router_.close();
      this.shutDown_ = true;
      return serialized;
    }.bind(this));
  };

  /**
   * Deserializes a SerializedDataSender.
   * @param {SerializedDataSender} serialized The serialized DataSender.
   * @return {!DataSender} The deserialized DataSender.
   */
  DataSender.deserialize = function(serialized) {
    var sender = $Object.create(DataSender.prototype);
    sender.deserialize_(serialized);
    return sender;
  };

  /**
   * Deserializes a SerializedDataSender into this DataSender.
   * @param {SerializedDataSender} serialized The serialized DataSender.
   * @private
   */
  DataSender.prototype.deserialize_ = function(serialized) {
    if (!serialized) {
      this.shutDown_ = true;
      return;
    }
    this.init_(
        serialized.sink, serialized.fatal_error_value, serialized.buffer_size);
  };

  /**
   * Sends data to the DataSink.
   * @return {!Promise.<number>} A promise to the number of bytes sent. If an
   *     error occurs, the promise will reject with an Error object with a
   *     property error containing the error code.
   * @throws Will throw if this has encountered a fatal error or a cancel is in
   *     progress.
   */
  DataSender.prototype.send = function(data) {
    if (this.shutDown_)
      throw new Error('DataSender has been closed');
    if (this.pendingCancel_)
      throw new Error('Cancel in progress');
    var send = new PendingSend(data);
    this.pendingSends_.push(send);
    this.sendInternal_();
    return send.getPromise();
  };

  DataSender.prototype.sendInternal_ = function() {
    while (this.pendingSends_.length && this.availableBufferCapacity_) {
      var result = this.pendingSends_[0].sendData(
          this.sink_, this.availableBufferCapacity_);
      this.availableBufferCapacity_ = result.remainingBufferCapacity;
      if (result.completed) {
        this.sendsAwaitingAck_.push(this.pendingSends_.shift());
      }
    }
  };

  /**
   * Requests the cancellation of any in-progress sends. Calls to
   * [send()]{@link module:data_sender.DataSender#send} will fail until the
   * cancel has completed.
   * @param {number} error The error to report for cancelled sends.
   * @return {!Promise} A promise that will resolve when the cancel completes.
   * @throws Will throw if this has encountered a fatal error or another cancel
   *     is in progress.
   */
  DataSender.prototype.cancel = function(error) {
    if (this.shutDown_)
      throw new Error('DataSender has been closed');
    if (this.pendingCancel_)
      throw new Error('Cancel already in progress');
    if (this.pendingSends_.length + this.sendsAwaitingAck_.length == 0)
      return Promise.resolve();

    this.sink_.cancel(error);
    this.cancelPromise_ = new Promise(function(resolve) {
      this.pendingCancel_ = resolve;
    }.bind(this));
    return this.cancelPromise_;
  };

  /**
   * Calls and clears the pending cancel callback if one is pending.
   * @private
   */
  DataSender.prototype.callCancelCallback_ = function() {
    if (this.pendingCancel_) {
      this.cancelPromise_ = null;
      this.pendingCancel_();
      this.pendingCancel_ = null;
    }
  };

  /**
   * Invoked by the DataSink to report that data has been successfully sent.
   * @param {number} numBytes The number of bytes sent.
   * @private
   */
  DataSender.prototype.reportBytesSent = function(numBytes) {
    this.availableBufferCapacity_ += numBytes;
    while (numBytes > 0 && this.sendsAwaitingAck_.length) {
      var result = this.sendsAwaitingAck_[0].reportBytesSent(numBytes);
      numBytes = result.bytesUnreported;
      if (result.done)
        this.sendsAwaitingAck_.shift();
    }
    if (numBytes > 0 && this.pendingSends_.length) {
      var result = this.pendingSends_[0].reportBytesSent(numBytes);
      numBytes = result.bytesUnreported;
    }
    // A cancel is completed when all of the sends that were in progress have
    // completed or failed. This is the case where all sends complete
    // successfully.
    if (this.pendingSends_.length + this.sendsAwaitingAck_.length == 0)
      this.callCancelCallback_();

    this.sendInternal_();
  };

  /**
   * Invoked by the DataSink to report an error in sending data.
   * @param {number} numBytes The number of bytes sent.
   * @param {number} error The error reported by the DataSink.
   * @private
   */
  DataSender.prototype.reportBytesSentAndError = function(numBytes, error) {
    this.availableBufferCapacity_ += numBytes;
    while (this.sendsAwaitingAck_.length) {
      var result = this.sendsAwaitingAck_[0].reportBytesSentAndError(
          numBytes, error);
      numBytes = result.bytesUnreported;
      this.sendsAwaitingAck_.shift();
      this.availableBufferCapacity_ += result.bytesToFlush;
    }
    while (this.pendingSends_.length) {
      var result = this.pendingSends_[0].reportBytesSentAndError(
          numBytes, error);
      numBytes = result.bytesUnreported;
      this.pendingSends_.shift();
      // Note: Only the first PendingSend in |pendingSends_| will have data to
      // flush as only the first can have sent data to the DataSink.
      this.availableBufferCapacity_ += result.bytesToFlush;
    }
    this.callCancelCallback_();
    return Promise.resolve();
  };

  return {DataSender: DataSender};
});
