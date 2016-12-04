// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "gin/test/expect",
    "mojo/public/js/bindings",
    "mojo/public/interfaces/bindings/tests/math_calculator.mojom",
    "mojo/public/js/threading",
    "gc",
], function(expect,
            bindings,
            math,
            threading,
            gc) {
  testIsBound()
      .then(testReusable)
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
    var binding = new bindings.Binding(math.Calculator, new CalculatorImpl());
    expect(binding.isBound()).toBeFalsy();

    var calc = new math.CalculatorPtr();
    var request = bindings.makeRequest(calc);
    binding.bind(request);
    expect(binding.isBound()).toBeTruthy();

    binding.close();
    expect(binding.isBound()).toBeFalsy();

    return Promise.resolve();
  }

  function testReusable() {
    var calc1 = new math.CalculatorPtr();
    var calc2 = new math.CalculatorPtr();

    var calcBinding = new bindings.Binding(math.Calculator,
                                           new CalculatorImpl(),
                                           bindings.makeRequest(calc1));

    var promise = calc1.add(2).then(function(response) {
      expect(response.value).toBe(2);
      calcBinding.bind(bindings.makeRequest(calc2));
      return calc2.add(2);
    }).then(function(response) {
      expect(response.value).toBe(4);
      return Promise.resolve();
    });

    return promise;
  }
});
