// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/apps/js/test/js_to_cpp_unittest", [
    "mojo/apps/js/test/js_to_cpp.mojom",
    "mojo/public/js/bindings/connection",
    "mojo/public/js/bindings/connector",
], function(jsToCpp, connection, connector) {
  var retainedConnection, kBadValue = 13;

  function JsSideConnection(cppSide) {
    this.cppSide_ = cppSide;
    cppSide.startTest();
  }

  JsSideConnection.prototype = Object.create(jsToCpp.JsSideStub.prototype);

  JsSideConnection.prototype.ping = function (arg) {
    this.cppSide_.pingResponse();

  };

  JsSideConnection.prototype.echo = function (numIterations, arg) {
    var arg2;
    var i;

    // Ensure negative values are negative.
    if (arg.si64 > 0)
      arg.si64 = kBadValue;

    if (arg.si32 > 0)
      arg.si32 = kBadValue;

    if (arg.si16 > 0)
      arg.si16 = kBadValue;

    if (arg.si8 > 0)
      arg.si8 = kBadValue;

    for (i = 0; i < numIterations; ++i) {
      arg2 = new jsToCpp.EchoArgs();
      arg2.si64 = -1;
      arg2.si32 = -1;
      arg2.si16 = -1;
      arg2.si8 = -1;
      arg2.name = "going";
      this.cppSide_.echoResponse(arg, arg2);
    }
    this.cppSide_.testFinished();
  };

  JsSideConnection.prototype.bitFlip = function (arg) {
    var iteration = 0;
    var stopSignalled = false;
    var proto = connector.Connector.prototype;
    proto.realAccept = proto.accept;
    proto.accept = function (message) {
      var offset = iteration / 8;
      var mask;
      var value;
      if (offset < message.buffer.arrayBuffer.byteLength) {
        mask = 1 << (iteration % 8);
        value = message.buffer.dataView.getUint8(offset) ^ mask;
        message.buffer.dataView.setUint8(offset, value);
        return this.realAccept(message);
      }
      stopSignalled = true;
      return false;
    };
    while (!stopSignalled) {
      this.cppSide_.bitFlipResponse(arg);
      iteration += 1;
    }
    proto.accept = proto.realAccept;
    proto.realAccept = null;
    this.cppSide_.testFinished();
  };

  return function(handle) {
    retainedConnection = new connection.Connection(handle, JsSideConnection,
                                                   jsToCpp.CppSideProxy);
  };
});
