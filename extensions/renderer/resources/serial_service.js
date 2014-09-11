// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('serial_service', [
    'content/public/renderer/service_provider',
    'data_receiver',
    'data_sender',
    'device/serial/serial.mojom',
    'mojo/public/js/bindings/core',
    'mojo/public/js/bindings/router',
], function(serviceProvider,
            dataReceiver,
            dataSender,
            serialMojom,
            core,
            routerModule) {
  /**
   * A Javascript client for the serial service and connection Mojo services.
   *
   * This provides a thick client around the Mojo services, exposing a JS-style
   * interface to serial connections and information about serial devices. This
   * converts parameters and result between the Apps serial API types and the
   * Mojo types.
   */

  var service = new serialMojom.SerialServiceProxy(new routerModule.Router(
      serviceProvider.connectToService(serialMojom.SerialServiceProxy.NAME_)));

  function getDevices() {
    return service.getDevices().then(function(response) {
      return $Array.map(response.devices, function(device) {
        var result = {path: device.path};
        if (device.has_vendor_id)
          result.vendorId = device.vendor_id;
        if (device.has_product_id)
          result.productId = device.product_id;
        if (device.display_name)
          result.displayName = device.display_name;
        return result;
      });
    });
  }

  var DEFAULT_CLIENT_OPTIONS = {
    persistent: false,
    name: '',
    receiveTimeout: 0,
    sendTimeout: 0,
    bufferSize: 4096,
  };

  var DATA_BITS_TO_MOJO = {
    undefined: serialMojom.DataBits.NONE,
    'seven': serialMojom.DataBits.SEVEN,
    'eight': serialMojom.DataBits.EIGHT,
  };
  var STOP_BITS_TO_MOJO = {
    undefined: serialMojom.StopBits.NONE,
    'one': serialMojom.StopBits.ONE,
    'two': serialMojom.StopBits.TWO,
  };
  var PARITY_BIT_TO_MOJO = {
    undefined: serialMojom.ParityBit.NONE,
    'no': serialMojom.ParityBit.NO,
    'odd': serialMojom.ParityBit.ODD,
    'even': serialMojom.ParityBit.EVEN,
  };
  var SEND_ERROR_TO_MOJO = {
    undefined: serialMojom.SendError.NONE,
    'disconnected': serialMojom.SendError.DISCONNECTED,
    'pending': serialMojom.SendError.PENDING,
    'timeout': serialMojom.SendError.TIMEOUT,
    'system_error': serialMojom.SendError.SYSTEM_ERROR,
  };
  var RECEIVE_ERROR_TO_MOJO = {
    undefined: serialMojom.ReceiveError.NONE,
    'disconnected': serialMojom.ReceiveError.DISCONNECTED,
    'device_lost': serialMojom.ReceiveError.DEVICE_LOST,
    'timeout': serialMojom.ReceiveError.TIMEOUT,
    'system_error': serialMojom.ReceiveError.SYSTEM_ERROR,
  };

  function invertMap(input) {
    var output = {};
    for (var key in input) {
      if (key == 'undefined')
        output[input[key]] = undefined;
      else
        output[input[key]] = key;
    }
    return output;
  }
  var DATA_BITS_FROM_MOJO = invertMap(DATA_BITS_TO_MOJO);
  var STOP_BITS_FROM_MOJO = invertMap(STOP_BITS_TO_MOJO);
  var PARITY_BIT_FROM_MOJO = invertMap(PARITY_BIT_TO_MOJO);
  var SEND_ERROR_FROM_MOJO = invertMap(SEND_ERROR_TO_MOJO);
  var RECEIVE_ERROR_FROM_MOJO = invertMap(RECEIVE_ERROR_TO_MOJO);

  function getServiceOptions(options) {
    var out = {};
    if (options.dataBits)
      out.data_bits = DATA_BITS_TO_MOJO[options.dataBits];
    if (options.stopBits)
      out.stop_bits = STOP_BITS_TO_MOJO[options.stopBits];
    if (options.parityBit)
      out.parity_bit = PARITY_BIT_TO_MOJO[options.parityBit];
    if ('ctsFlowControl' in options) {
      out.has_cts_flow_control = true;
      out.cts_flow_control = options.ctsFlowControl;
    }
    if ('bitrate' in options)
      out.bitrate = options.bitrate;
    return out;
  }

  function convertServiceInfo(result) {
    if (!result.info)
      throw new Error('Failed to get ConnectionInfo.');
    return {
      ctsFlowControl: !!result.info.cts_flow_control,
      bitrate: result.info.bitrate || undefined,
      dataBits: DATA_BITS_FROM_MOJO[result.info.data_bits],
      stopBits: STOP_BITS_FROM_MOJO[result.info.stop_bits],
      parityBit: PARITY_BIT_FROM_MOJO[result.info.parity_bit],
    };
  }

  function Connection(
      remoteConnection, router, receivePipe, sendPipe, id, options) {
    this.remoteConnection_ = remoteConnection;
    this.router_ = router;
    this.options_ = {};
    for (var key in DEFAULT_CLIENT_OPTIONS) {
      this.options_[key] = DEFAULT_CLIENT_OPTIONS[key];
    }
    this.setClientOptions_(options);
    this.receivePipe_ =
        new dataReceiver.DataReceiver(receivePipe,
                                      this.options_.bufferSize,
                                      serialMojom.ReceiveError.DISCONNECTED);
    this.sendPipe_ = new dataSender.DataSender(
        sendPipe, this.options_.bufferSize, serialMojom.SendError.DISCONNECTED);
    this.id_ = id;
    getConnections().then(function(connections) {
      connections[this.id_] = this;
    }.bind(this));
    this.paused_ = false;
    this.sendInProgress_ = false;

    // queuedReceiveData_ or queuedReceiveError will store the receive result or
    // error, respectively, if a receive completes or fails while this
    // connection is paused. At most one of the the two may be non-null: a
    // receive completed while paused will only set one of them, no further
    // receives will be performed while paused and a queued result is dispatched
    // before any further receives are initiated when unpausing.
    this.queuedReceiveData_ = null;
    this.queuedReceiveError = null;

    this.startReceive_();
  }

  Connection.create = function(path, options) {
    options = options || {};
    var serviceOptions = getServiceOptions(options);
    var pipe = core.createMessagePipe();
    var sendPipe = core.createMessagePipe();
    var receivePipe = core.createMessagePipe();
    service.connect(path,
                    serviceOptions,
                    pipe.handle0,
                    sendPipe.handle0,
                    receivePipe.handle0);
    var router = new routerModule.Router(pipe.handle1);
    var connection = new serialMojom.ConnectionProxy(router);
    return connection.getInfo().then(convertServiceInfo).then(function(info) {
      return Promise.all([info, allocateConnectionId()]);
    }).catch(function(e) {
      router.close();
      core.close(sendPipe.handle1);
      core.close(receivePipe.handle1);
      throw e;
    }).then(function(results) {
      var info = results[0];
      var id = results[1];
      var serialConnectionClient = new Connection(connection,
                                                  router,
                                                  receivePipe.handle1,
                                                  sendPipe.handle1,
                                                  id,
                                                  options);
      var clientInfo = serialConnectionClient.getClientInfo_();
      for (var key in clientInfo) {
        info[key] = clientInfo[key];
      }
      return {
        connection: serialConnectionClient,
        info: info,
      };
    });
  };

  Connection.prototype.close = function() {
    this.router_.close();
    this.receivePipe_.close();
    this.sendPipe_.close();
    clearTimeout(this.receiveTimeoutId_);
    clearTimeout(this.sendTimeoutId_);
    return getConnections().then(function(connections) {
      delete connections[this.id_];
      return true;
    }.bind(this));
  };

  Connection.prototype.getClientInfo_ = function() {
    var info = {
      connectionId: this.id_,
      paused: this.paused_,
    };
    for (var key in this.options_) {
      info[key] = this.options_[key];
    }
    return info;
  };

  Connection.prototype.getInfo = function() {
    var info = this.getClientInfo_();
    return this.remoteConnection_.getInfo().then(convertServiceInfo).then(
        function(result) {
      for (var key in result) {
        info[key] = result[key];
      }
      return info;
    }).catch(function() {
      return info;
    });
  };

  Connection.prototype.setClientOptions_ = function(options) {
    if ('name' in options)
      this.options_.name = options.name;
    if ('receiveTimeout' in options)
      this.options_.receiveTimeout = options.receiveTimeout;
    if ('sendTimeout' in options)
      this.options_.sendTimeout = options.sendTimeout;
    if ('bufferSize' in options)
      this.options_.bufferSize = options.bufferSize;
  };

  Connection.prototype.setOptions = function(options) {
    this.setClientOptions_(options);
    var serviceOptions = getServiceOptions(options);
    if ($Object.keys(serviceOptions).length == 0)
      return true;
    return this.remoteConnection_.setOptions(serviceOptions).then(
        function(result) {
      return !!result.success;
    }).catch(function() {
      return false;
    });
  };

  Connection.prototype.getControlSignals = function() {
    return this.remoteConnection_.getControlSignals().then(function(result) {
      if (!result.signals)
        throw new Error('Failed to get control signals.');
      var signals = result.signals;
      return {
        dcd: !!signals.dcd,
        cts: !!signals.cts,
        ri: !!signals.ri,
        dsr: !!signals.dsr,
      };
    });
  };

  Connection.prototype.setControlSignals = function(signals) {
    var controlSignals = {};
    if ('dtr' in signals) {
      controlSignals.has_dtr = true;
      controlSignals.dtr = signals.dtr;
    }
    if ('rts' in signals) {
      controlSignals.has_rts = true;
      controlSignals.rts = signals.rts;
    }
    return this.remoteConnection_.setControlSignals(controlSignals).then(
        function(result) {
      return !!result.success;
    });
  };

  Connection.prototype.flush = function() {
    return this.remoteConnection_.flush().then(function(result) {
      return !!result.success;
    });
  };

  Connection.prototype.setPaused = function(paused) {
    this.paused_ = paused;
    if (paused) {
      clearTimeout(this.receiveTimeoutId_);
      this.receiveTimeoutId_ = null;
    } else if (!this.receiveInProgress_) {
      this.startReceive_();
    }
  };

  Connection.prototype.send = function(data) {
    if (this.sendInProgress_)
      return Promise.resolve({bytesSent: 0, error: 'pending'});

    if (this.options_.sendTimeout) {
      this.sendTimeoutId_ = setTimeout(function() {
        this.sendPipe_.cancel(serialMojom.SendError.TIMEOUT);
      }.bind(this), this.options_.sendTimeout);
    }
    this.sendInProgress_ = true;
    return this.sendPipe_.send(data).then(function(bytesSent) {
      return {bytesSent: bytesSent};
    }).catch(function(e) {
      return {
        bytesSent: e.bytesSent,
        error: SEND_ERROR_FROM_MOJO[e.error],
      };
    }).then(function(result) {
      if (this.sendTimeoutId_)
        clearTimeout(this.sendTimeoutId_);
      this.sendTimeoutId_ = null;
      this.sendInProgress_ = false;
      return result;
    }.bind(this));
  };

  Connection.prototype.startReceive_ = function() {
    this.receiveInProgress_ = true;
    var receivePromise = null;
    // If we have a queued receive result, dispatch it immediately instead of
    // starting a new receive.
    if (this.queuedReceiveData_) {
      receivePromise = Promise.resolve(this.queuedReceiveData_);
      this.queuedReceiveData_ = null;
    } else if (this.queuedReceiveError) {
      receivePromise = Promise.reject(this.queuedReceiveError);
      this.queuedReceiveError = null;
    } else {
      receivePromise = this.receivePipe_.receive();
    }
    receivePromise.then(this.onDataReceived_.bind(this)).catch(
        this.onReceiveError_.bind(this));
    this.startReceiveTimeoutTimer_();
  };

  Connection.prototype.onDataReceived_ = function(data) {
    this.startReceiveTimeoutTimer_();
    this.receiveInProgress_ = false;
    if (this.paused_) {
      this.queuedReceiveData_ = data;
      return;
    }
    if (this.onData) {
      this.onData(data);
    }
    if (!this.paused_) {
      this.startReceive_();
    }
  };

  Connection.prototype.onReceiveError_ = function(e) {
    clearTimeout(this.receiveTimeoutId_);
    this.receiveInProgress_ = false;
    if (this.paused_) {
      this.queuedReceiveError = e;
      return;
    }
    var error = e.error;
    this.paused_ = true;
    if (this.onError)
      this.onError(RECEIVE_ERROR_FROM_MOJO[error]);
  };

  Connection.prototype.startReceiveTimeoutTimer_ = function() {
    clearTimeout(this.receiveTimeoutId_);
    if (this.options_.receiveTimeout && !this.paused_) {
      this.receiveTimeoutId_ = setTimeout(this.onReceiveTimeout_.bind(this),
                                          this.options_.receiveTimeout);
    }
  };

  Connection.prototype.onReceiveTimeout_ = function() {
    if (this.onError)
      this.onError('timeout');
    this.startReceiveTimeoutTimer_();
  };

  var connections_ = {};
  var nextConnectionId_ = 0;

  // Wrap all access to |connections_| through getConnections to avoid adding
  // any synchronous dependencies on it. This will likely be important when
  // supporting persistent connections by stashing them.
  function getConnections() {
    return Promise.resolve(connections_);
  }

  function getConnection(id) {
    return getConnections().then(function(connections) {
      if (!connections[id])
        throw new Error('Serial connection not found.');
      return connections[id];
    });
  }

  function allocateConnectionId() {
    return Promise.resolve(nextConnectionId_++);
  }

  return {
    getDevices: getDevices,
    createConnection: Connection.create,
    getConnection: getConnection,
    getConnections: getConnections,
    // For testing.
    Connection: Connection,
  };
});
