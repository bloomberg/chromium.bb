// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testRunner = testRunner || {};

(function() {
  native function NotifyDone();
  native function OverridePreference();
  native function SetDumpAsText();
  native function SetDumpChildFramesAsText();
  native function SetWaitUntilDone();

  native function NotImplemented();

  var DefaultHandler = function(name) {
    var handler = {
      get: function(receiver, property) {
        if (property === "splice")
          return undefined;
        if (property === "__proto__")
          return {}
        if (property === "toString")
          return function() { return "[object Object]"; };
        if (property === "constructor" || property === "valueOf")
          return function() { return {}; };
        NotImplemented(name, property);
        return function() {}
      },
      getOwnPropertyNames: function() {
        return [];
      },
      getPropertyDescriptor: function(property) {
        NotImplemented(name, property);
        return undefined;
      }
    }
    return Proxy.create(handler);
  }

  var TestRunner = function() {
    Object.defineProperty(this, "notifyDone", {value: NotifyDone});
    Object.defineProperty(this, "dumpAsText", {value: SetDumpAsText});
    Object.defineProperty(this,
                          "dumpChildFramesAsText",
                          {value: SetDumpChildFramesAsText});
    Object.defineProperty(this, "waitUntilDone", {value: SetWaitUntilDone});
    Object.defineProperty(this,
                          "overridePreference",
                          {value: OverridePreference});

  }
  TestRunner.prototype = DefaultHandler("testRunner");
  testRunner = new TestRunner();
})();
