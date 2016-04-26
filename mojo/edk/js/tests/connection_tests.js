// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "gin/test/expect",
    "mojo/public/js/connection",
    "mojo/public/js/core",
    "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom",
    "mojo/public/interfaces/bindings/tests/sample_service.mojom",
    "mojo/public/js/threading",
    "gc",
], function(expect,
            connection,
            core,
            sample_interfaces,
            sample_service,
            threading,
            gc) {
  testClientServer()
      .then(testWriteToClosedPipe)
      .then(testRequestResponse)
      .then(function() {
    this.result = "PASS";
    gc.collectGarbage();  // should not crash
    threading.quit();
  }.bind(this)).catch(function(e) {
    this.result = "FAIL: " + (e.stack || e);
    threading.quit();
  }.bind(this));

  function testClientServer() {
    // ServiceImpl ------------------------------------------------------------

    function ServiceImpl() {
    }

    ServiceImpl.prototype = Object.create(
        sample_service.Service.stubClass.prototype);

    ServiceImpl.prototype.frobinate = function(foo, baz, port) {
      expect(foo.name).toBe("Example name");
      expect(baz).toBe(sample_service.Service.BazOptions.REGULAR);
      expect(core.close(port)).toBe(core.RESULT_OK);

      return Promise.resolve({result: 42});
    };

    var pipe = core.createMessagePipe();
    var anotherPipe = core.createMessagePipe();
    var sourcePipe = core.createMessagePipe();

    var connection0 = new connection.Connection(pipe.handle0, ServiceImpl);

    var connection1 = new connection.Connection(
        pipe.handle1, undefined, sample_service.Service.proxyClass);

    var foo = new sample_service.Foo();
    foo.bar = new sample_service.Bar();
    foo.name = "Example name";
    foo.source = sourcePipe.handle0;
    var promise = connection1.remote.frobinate(
        foo, sample_service.Service.BazOptions.REGULAR, anotherPipe.handle0)
            .then(function(response) {
      expect(response.result).toBe(42);

      connection0.close();
      connection1.close();

      return Promise.resolve();
    });

    // sourcePipe.handle1 hasn't been closed yet.
    expect(core.close(sourcePipe.handle1)).toBe(core.RESULT_OK);

    // anotherPipe.handle1 hasn't been closed yet.
    expect(core.close(anotherPipe.handle1)).toBe(core.RESULT_OK);

    return promise;
  }

  function testWriteToClosedPipe() {
    var pipe = core.createMessagePipe();
    var anotherPipe = core.createMessagePipe();

    var connection1 = new connection.Connection(
        pipe.handle1, function() {}, sample_service.Service.proxyClass);

    // Close the other end of the pipe.
    core.close(pipe.handle0);

    // Not observed yet because we haven't pumped events yet.
    expect(connection1.encounteredError()).toBeFalsy();

    var promise = connection1.remote.frobinate(
        null, sample_service.Service.BazOptions.REGULAR, anotherPipe.handle0)
            .then(function(response) {
      return Promise.reject("Unexpected response");
    }).catch(function(e) {
      // We should observe the closed pipe.
      expect(connection1.encounteredError()).toBeTruthy();
      return Promise.resolve();
    });

    // Write failures are not reported.
    expect(connection1.encounteredError()).toBeFalsy();

    return promise;
  }

  function testRequestResponse() {
    // ProviderImpl ------------------------------------------------------------

    function ProviderImpl() {
    }

    ProviderImpl.prototype =
        Object.create(sample_interfaces.Provider.stubClass.prototype);

    ProviderImpl.prototype.echoString = function(a) {
      return Promise.resolve({a: a});
    };

    ProviderImpl.prototype.echoStrings = function(a, b) {
      return Promise.resolve({a: a, b: b});
    };

    var pipe = core.createMessagePipe();

    var connection0 = new connection.Connection(pipe.handle0, ProviderImpl);

    var connection1 = new connection.Connection(
        pipe.handle1, undefined, sample_interfaces.Provider.proxyClass);

    var promise = connection1.remote.echoString("hello")
        .then(function(response) {
      expect(response.a).toBe("hello");
      return connection1.remote.echoStrings("hello", "world");
    }).then(function(response) {
      expect(response.a).toBe("hello");
      expect(response.b).toBe("world");
      // Mock a read failure, expect it to fail.
      core.readMessage = function() {
        return { result: core.RESULT_UNKNOWN };
      };
      return connection1.remote.echoString("goodbye");
    }).then(function() {
      throw Error("Expected echoString to fail.");
    }, function(error) {
      expect(error.message).toBe("Connection error: " + core.RESULT_UNKNOWN);

      return Promise.resolve();
    });

    return promise;
  }
});
