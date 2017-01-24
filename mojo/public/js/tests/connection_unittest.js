// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "gin/test/expect",
    "mojo/public/js/bindings",
    "mojo/public/js/core",
    "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom",
    "mojo/public/interfaces/bindings/tests/sample_service.mojom",
    "mojo/public/js/threading",
    "gc",
], function(expect,
            bindings,
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

    ServiceImpl.prototype.frobinate = function(foo, baz, port) {
      expect(foo.name).toBe("Example name");
      expect(baz).toBe(sample_service.Service.BazOptions.REGULAR);
      expect(port.ptr.isBound()).toBeTruthy();
      port.ptr.reset();

      return Promise.resolve({result: 42});
    };

    var service = new sample_service.ServicePtr();
    var serviceBinding = new bindings.Binding(sample_service.Service,
                                              new ServiceImpl(),
                                              bindings.makeRequest(service));
    var sourcePipe = core.createMessagePipe();
    var port = new sample_service.PortPtr();
    var portRequest = bindings.makeRequest(port);

    var foo = new sample_service.Foo();
    foo.bar = new sample_service.Bar();
    foo.name = "Example name";
    foo.source = sourcePipe.handle0;
    var promise = service.frobinate(
        foo, sample_service.Service.BazOptions.REGULAR, port)
            .then(function(response) {
      expect(response.result).toBe(42);

      service.ptr.reset();
      serviceBinding.close();

      return Promise.resolve();
    });

    // sourcePipe.handle1 hasn't been closed yet.
    expect(core.close(sourcePipe.handle1)).toBe(core.RESULT_OK);

    // portRequest.handle hasn't been closed yet.
    expect(core.close(portRequest.handle)).toBe(core.RESULT_OK);

    return promise;
  }

  function testWriteToClosedPipe() {
    var service = new sample_service.ServicePtr();
    // Discard the interface request.
    bindings.makeRequest(service);
    gc.collectGarbage();

    var promise = service.frobinate(
        null, sample_service.Service.BazOptions.REGULAR, null)
            .then(function(response) {
      return Promise.reject("Unexpected response");
    }).catch(function(e) {
      // We should observe the closed pipe.
      return Promise.resolve();
    });

    return promise;
  }

  function testRequestResponse() {
    // ProviderImpl ------------------------------------------------------------

    function ProviderImpl() {
    }

    ProviderImpl.prototype.echoString = function(a) {
      return Promise.resolve({a: a});
    };

    ProviderImpl.prototype.echoStrings = function(a, b) {
      return Promise.resolve({a: a, b: b});
    };

    var provider = new sample_interfaces.ProviderPtr();
    var providerBinding = new bindings.Binding(sample_interfaces.Provider,
                                               new ProviderImpl(),
                                               bindings.makeRequest(provider));
    var promise = provider.echoString("hello").then(function(response) {
      expect(response.a).toBe("hello");
      return provider.echoStrings("hello", "world");
    }).then(function(response) {
      expect(response.a).toBe("hello");
      expect(response.b).toBe("world");
      // Mock a read failure, expect it to fail.
      core.readMessage = function() {
        return { result: core.RESULT_UNKNOWN };
      };
      return provider.echoString("goodbye");
    }).then(function() {
      throw Error("Expected echoString to fail.");
    }, function(error) {
      expect(error.message).toBe("Connection error: " + core.RESULT_UNKNOWN);

      return Promise.resolve();
    });

    return promise;
  }
});
