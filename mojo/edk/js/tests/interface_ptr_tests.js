// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "gin/test/expect",
    "mojo/public/js/bindings",
    "mojo/public/js/core",
    "mojo/public/interfaces/bindings/tests/math_calculator.mojom",
    "mojo/public/js/threading",
    "gc",
], function(expect,
            bindings,
            core,
            math,
            threading,
            gc) {
  testIsBound()
      .then(testEndToEnd)
      .then(testReusable)
      .then(testConnectionError)
      .then(testPassInterface)
      .then(testBindRawHandle)
      .then(function() {
    this.result = "PASS";
    gc.collectGarbage();  // should not crash
    threading.quit();
  }.bind(this)).catch(function(e) {
    this.result = "FAIL: " + (e.stack || e);
    threading.quit();
  }.bind(this));

  function CalculatorImpl() {
    this.total = 0;
  }

  CalculatorImpl.prototype.clear = function() {
    this.total = 0;
    return Promise.resolve({value: this.total});
  };

  CalculatorImpl.prototype.add = function(value) {
    this.total += value;
    return Promise.resolve({value: this.total});
  };

  CalculatorImpl.prototype.multiply = function(value) {
    this.total *= value;
    return Promise.resolve({value: this.total});
  };

  function testIsBound() {
    var calc = new math.CalculatorPtr();
    expect(calc.ptr.isBound()).toBeFalsy();

    var request = bindings.makeRequest(calc);
    expect(calc.ptr.isBound()).toBeTruthy();

    calc.ptr.reset();
    expect(calc.ptr.isBound()).toBeFalsy();

    return Promise.resolve();
  }

  function testEndToEnd() {
    var calc = new math.CalculatorPtr();
    var calcBinding = new bindings.Binding(math.Calculator,
                                           new CalculatorImpl(),
                                           bindings.makeRequest(calc));

    var promise = calc.add(2).then(function(response) {
      expect(response.value).toBe(2);
      return calc.multiply(5);
    }).then(function(response) {
      expect(response.value).toBe(10);
      return calc.clear();
    }).then(function(response) {
      expect(response.value).toBe(0);
      return Promise.resolve();
    });

    return promise;
  }

  function testReusable() {
    var calc = new math.CalculatorPtr();
    var calcImpl1 = new CalculatorImpl();
    var calcBinding1 = new bindings.Binding(math.Calculator,
                                            calcImpl1,
                                            bindings.makeRequest(calc));
    var calcImpl2 = new CalculatorImpl();
    var calcBinding2 = new bindings.Binding(math.Calculator, calcImpl2);

    var promise = calc.add(2).then(function(response) {
      expect(response.value).toBe(2);
      calcBinding2.bind(bindings.makeRequest(calc));
      return calc.add(2);
    }).then(function(response) {
      expect(response.value).toBe(2);
      expect(calcImpl1.total).toBe(2);
      expect(calcImpl2.total).toBe(2);
      return Promise.resolve();
    });

    return promise;
  }

  function testConnectionError() {
    var calc = new math.CalculatorPtr();
    var calcBinding = new bindings.Binding(math.Calculator,
                                           new CalculatorImpl(),
                                           bindings.makeRequest(calc));

    var promise = new Promise(function(resolve, reject) {
      calc.ptr.setConnectionErrorHandler(function() {
        resolve();
      });
      calcBinding.close();
    });

    return promise;
  }

  function testPassInterface() {
    var calc = new math.CalculatorPtr();
    var newCalc = null;
    var calcBinding = new bindings.Binding(math.Calculator,
                                           new CalculatorImpl(),
                                           bindings.makeRequest(calc));

    var promise = calc.add(2).then(function(response) {
      expect(response.value).toBe(2);
      newCalc = new math.CalculatorPtr();
      newCalc.ptr.bind(calc.ptr.passInterface());
      expect(calc.ptr.isBound()).toBeFalsy();
      return newCalc.add(2);
    }).then(function(response) {
      expect(response.value).toBe(4);
      return Promise.resolve();
    });

    return promise;
  }

  function testBindRawHandle() {
    var pipe = core.createMessagePipe();
    var calc = new math.CalculatorPtr(pipe.handle0);
    var newCalc = null;
    var calcBinding = new bindings.Binding(math.Calculator,
                                           new CalculatorImpl(),
                                           pipe.handle1);

    var promise = calc.add(2).then(function(response) {
      expect(response.value).toBe(2);
      return Promise.resolve();
    });

    return promise;
  }
});
