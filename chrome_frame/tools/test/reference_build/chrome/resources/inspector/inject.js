// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Javascript that is being injected into the inspectable page
 * while debugging.
 */
goog.provide('devtools.Injected');


/**
 * Main injected object.
 * @constructor.
 */
devtools.Injected = function() {
};


/**
 * Dispatches given method with given args on the host object.
 * @param {string} method Method name.
 */
devtools.Injected.prototype.InspectorController = function(method, var_args) {
  var args = Array.prototype.slice.call(arguments, 1);
  return InspectorController[method].apply(InspectorController, args);
};


/**
 * Dispatches given method with given args on the InjectedScript.
 * @param {string} method Method name.
 */
devtools.Injected.prototype.InjectedScript = function(method, var_args) {
  var args = Array.prototype.slice.call(arguments, 1);
  var result = InjectedScript[method].apply(InjectedScript, args);
  return result;
};


// Plugging into upstreamed support.
InjectedScript._window = function() {
  return contentWindow;
};


// Plugging into upstreamed support.
Object.className = function(obj) {
  return (obj == null) ? "null" : obj.constructor.name;
};
