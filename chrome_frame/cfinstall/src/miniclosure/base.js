// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implements a shim so that the CFInstall scripts can be compiled
 * with or without Closure. In particular, chromeframe.js is used by the stub,
 * the implementation, and the download site, so we need to provide an
 * implementation of goog.provide.
 **/

var goog = {};
goog.global = this;

/**
 * From closure/base.js:goog.exportPath_ .
 * @param {string} name
 * @param {Object=} opt_object
 */
goog.provide = function(name, opt_object) {
  var parts = name.split('.');
  var cur = goog.global;

  // Internet Explorer exhibits strange behavior when throwing errors from
  // methods externed in this manner.  See the testExportSymbolExceptions in
  // base_test.html for an example.
  if (!(parts[0] in cur) && cur.execScript)
    cur.execScript('var ' + parts[0]);

  // Certain browsers cannot parse code in the form for((a in b); c;);
  // This pattern is produced by the JSCompiler when it collapses the
  // statement above into the conditional loop below. To prevent this from
  // happening, use a for-loop and reserve the init logic as below.

  // Parentheses added to eliminate strict JS warning in Firefox.
  for (var part; parts.length && (part = parts.shift());) {
    if (!parts.length && opt_object !== undefined) {
      // last part and we have an object; use it
      cur[part] = opt_object;
    } else if (cur[part]) {
      cur = cur[part];
    } else {
      cur = cur[part] = {};
    }
  }
};

// The following line causes the closureBuilder script to recognize this as
// base.js .
goog.provide('goog');

/**
 * From closure/base.js:goog.exportPath_ .
 * @param {string} name
 * @param {Object=} opt_object
 */
goog.exportSymbol = goog.provide;

/**
 * NO-OP
 * @param {string} name
 */
goog.require = function(name) {};

/**
 * A simple form that supports only bound 'this', not arguments.
 * @param {Function} fn A function to partially apply.
 * @param {Object|undefined} selfObj Specifies the object which |this| should
 *     point to when the function is run.
 * @return {!Function} A partially-applied form of the function bind() was
 *     invoked as a method of.
 */
goog.bind = function(fn, selfObj) {
  return function() {
    return fn.apply(selfObj, arguments);
  };
};
