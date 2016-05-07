// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('__crWeb.webUIModuleLoadNotifier');

/**
 * A holder class for require.js contexts whose load is pending.
 * @constructor
 */
var WebUIModuleLoadNotifier = function() {
  this.lastUsedId_ = 0;
  this.pendingContexts_ = new Map();
};

/**
 * Adds the given context to the list of pending contexts.
 * @param {string} name Name of the module that will be loaded.
 * @param {!Object} context Require.js context to hold.
 * @return {string} Identifier for the added context, can be later used for
 *     {@code moduleLoadCompleted} and {@code moduleLoadFailed} arguments.
 */
WebUIModuleLoadNotifier.prototype.addPendingContext = function(name, context) {
  this.lastUsedId_++;
  var key = name + this.lastUsedId_;
  this.pendingContexts_.set(key, context);
  return String(this.lastUsedId_);
};

/**
 * Signals that module has successfully loaded.
 * @param {string} name Name of the module that has been loaded.
 * @param {String} loadId Identifier returned from {@code addPendingContext}.
 */
WebUIModuleLoadNotifier.prototype.moduleLoadCompleted = function(name, loadId) {
  this.popContext_(name, loadId).completeLoad(name);
};

/**
 * Signals that module has failed to load.
 * @param {string} name Name of the module that has failed to load.
 * @param {String} loadId Identifier returned from {@code addPendingContext}.
 */
WebUIModuleLoadNotifier.prototype.moduleLoadFailed = function(name, loadId) {
  this.popContext_(name, loadId).onScriptFailure(name);
};

/**
 * Pops the pending context.
 * @param {string} name Name of the module that has failed to load.
 * @param {String} loadId Identifier returned from {@code addPendingContext}.
 * @return {Object} Require.js context
 */
WebUIModuleLoadNotifier.prototype.popContext_ = function(name, loadId) {
  var key = name + loadId;
  var context = this.pendingContexts_.get(key);
  this.pendingContexts_.delete(key);
  return context;
};
