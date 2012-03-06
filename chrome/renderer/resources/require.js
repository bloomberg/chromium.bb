// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file should define a function that returns a require() method that can
// be used to load JS modules.
//
// A module works the same as modules in node.js - JS script that populates
// an exports object, which is what require()ers get returned. The JS in a
// module will be run at most once as the exports result is cached.
//
// Modules have access to the require() function which can be used to load
// other JS dependencies, and requireNative() which returns objects that
// define native handlers.
//
// |bootstrap| contains the following fields:
//   - GetSource(module): returns the source code for |module|
//   - Run(source): runs the string os JS |source|
//   - GetNative(nativeName): returns the named native object
(function(bootstrap) {

function wrap(source) {
  return "(function(require, requireNative, exports) {" + source +
      "\n})";
}

function ModuleLoader() {
  this.cache_ = {};
};

ModuleLoader.prototype.run = function(id) {
  var source = bootstrap.GetSource(id);
  if (!source) {
    return null;
  }
  var wrappedSource = wrap(source);
  var f = bootstrap.Run(wrappedSource);
  var exports = {};
  f(require, requireNative, exports);
  return exports;
};

ModuleLoader.prototype.load = function(id) {
  var exports = this.cache_[id];
  if (exports) {
    return exports;
  }
  exports = this.run(id);
  this.cache_[id] = exports;
  return exports;
};

var moduleLoader = new ModuleLoader();

function requireNative(nativeName) {
  return bootstrap.GetNative(nativeName);
}

function require(moduleId) {
  return moduleLoader.load(moduleId);
}

return require;
});
