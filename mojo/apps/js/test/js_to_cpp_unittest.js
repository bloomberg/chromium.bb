// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/apps/js/test/js_to_cpp_unittest", [
    "mojo/apps/js/test/js_to_cpp.mojom",
    "mojo/public/js/bindings/connection",
], function(jsToCpp, connector) {
  var connection, kIterations = 100, kBadValue = 13;

  function JsSideConnection(cppSide) {
    this.cppSide_ = cppSide;
    cppSide.startTest();
  }

  JsSideConnection.prototype = Object.create(jsToCpp.JsSideStub.prototype);

  JsSideConnection.prototype.ping = function (arg) {
    this.cppSide_.pingResponse();

  };

  JsSideConnection.prototype.echo = function (arg) {
    var i, arg2;

    // Ensure negative values are negative.
    if (arg.si64 > 0)
      arg.si64 = kBadValue;

    if (arg.si32 > 0)
      arg.si32 = kBadValue;

    if (arg.si16 > 0)
      arg.si16 = kBadValue;

    if (arg.si8 > 0)
      arg.si8 = kBadValue;

    for (i = 0; i < kIterations; ++i) {
      arg2 = new jsToCpp.EchoArgs();
      arg2.si64 = -1;
      arg2.si32 = -1;
      arg2.si16 = -1;
      arg2.si8 = -1;
      arg2.name = "going";
      this.cppSide_.echoResponse(arg, arg2);
    }
  };

  return function(handle) {
    connection = new connector.Connection(handle, JsSideConnection,
                                          jsToCpp.CppSideProxy);
  };
});
