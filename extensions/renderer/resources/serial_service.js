// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('serial_service', [
    'content/public/renderer/service_provider',
    'device/serial/serial.mojom',
    'mojo/public/js/bindings/core',
    'mojo/public/js/bindings/router',
], function(serviceProvider, serialMojom, core, routerModule) {
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
        var result = {path: device.path || ''};
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

  function Connection(remoteConnection, router, id, options) {
    this.remoteConnection_ = remoteConnection;
    this.router_ = router;
    this.id_ = id;
    getConnections().then(function(connections) {
      connections[this.id_] = this;
    }.bind(this));
    this.paused_ = false;
    this.options_ = {};
    for (var key in DEFAULT_CLIENT_OPTIONS) {
      this.options_[key] = DEFAULT_CLIENT_OPTIONS[key];
    }
    this.setClientOptions_(options);
  }

  Connection.create = function(path, options) {
    options = options || {};
    var serviceOptions = getServiceOptions(options);
    var pipe = core.createMessagePipe();
    service.connect(path, serviceOptions, pipe.handle0);
    var router = new routerModule.Router(pipe.handle1);
    var connection = new serialMojom.ConnectionProxy(router);
    return connection.getInfo().then(convertServiceInfo).then(
        function(info) {
      return Promise.all([info, allocateConnectionId()]);
    }).catch(function(e) {
      router.close();
      throw e;
    }).then(function(results) {
      var info = results[0];
      var id = results[1];
      var serialConnectionClient = new Connection(
          connection, router, id, options);
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
    return getConnections().then(function(connections) {
      delete connections[this.id_]
      return true;
    }.bind(this));
  };

  Connection.prototype.getClientInfo_ = function() {
    var info = {
      connectionId: this.id_,
      paused: this.paused_,
    }
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
        throw new Error ('Serial connection not found.');
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
  };
});
