// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// externs.js contains variable declarations for the closure compiler
// This isn't actually used when running the code.

// JSCompiler doesn't know about this new Element property
Element.prototype.classList = {};
/** @param {string} c */
Element.prototype.classList.remove = function(c) {};
/** @param {string} c */
Element.prototype.classList.add = function(c) {};
/** @param {string} c */
Element.prototype.classList.contains = function(c) {};

/**
 *  @constructor
 *  @extends {Event}
 */
var CustomEvent = function() {};
CustomEvent.prototype.initCustomEvent =
  function(eventType, bubbles, cancellable, detail) {};
/** @type {TouchHandler.EventDetail} */
CustomEvent.prototype.detail;


/** @param {string} s
 *  @return {string}
 */
var url = function(s) {};

/**
 * @param {string} type
 * @param {EventListener|function(Event):(boolean|undefined)} listener
 * @param {boolean=} opt_useCapture
 * @return {undefined}
 * @suppress {checkTypes}
 */
Window.prototype.addEventListener = function(type, listener, opt_useCapture) {};
