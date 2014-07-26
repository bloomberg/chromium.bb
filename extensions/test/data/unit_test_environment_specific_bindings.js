// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var nativesPromise = requireAsync('testNatives');

function registerHooks(api) {
  var chromeTest = api.compiledApi;
  var apiFunctions = api.apiFunctions;

  apiFunctions.setHandleRequest('notifyPass', function() {
    nativesPromise.then(function(natives) {
      natives.NotifyPass();
    });
  });

  apiFunctions.setHandleRequest('notifyFail', function(message) {
    nativesPromise.then(function(natives) {
      natives.NotifyFail(message);
    });
  });

  apiFunctions.setHandleRequest('log', function() {
    nativesPromise.then(function(natives) {
      natives.Log($Array.join(arguments, ' '));
    });
  });

}

function testDone(runNextTest) {
    // Use a promise here to allow previous test contexts to be eligible for
    // garbage collection.
    Promise.resolve().then(function() {
      runNextTest();
    });
}

function exportTests(tests, runTests, exports) {
  $Array.forEach(tests, function(test) {
    exports[test.name] = function() {
      runTests([test]);
      return true;
    }
  });
}

exports.registerHooks = registerHooks;
exports.testDone = testDone;
exports.exportTests = exportTests;
