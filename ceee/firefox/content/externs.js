// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'externs' file for JSCompiler use with Firefox CEEE javascript
 * code loaded from overlay.xul.
 */

// Defined in CEEE global module
var CEEE_bookmarks
var CEEE_globals
var CEEE_ioService
var CEEE_json
var CEEE_mozilla_tabs
var CEEE_mozilla_windows

// Javascript predefined variables
var arguments;
var undefined;

// DOM predefined variables
var document;
var window;

// Javascript/DOM builtin function
function parseInt(str, radix) {}
function open(utl, target, features) {}


// Javasript runtime classes
/**
 * @returns string
 * @constructor
 */
function Date(opt_a, opt_b, opt_c, opt_d, opt_e, opt_f, opt_g) {}
/**
 * @param {*} opt_a
 * @param {*} opt_b
 * @param {*} opt_c
 * @returns !Error
 * @constructor
 */
function Error(opt_a, opt_b, opt_c) {}
/**
 * @param {*} opt_a
 * @param {*} opt_b
 * @returns !RegExp
 * @constructor
 */
function RegExp(opt_a, opt_b) {}
/**
 * @param {*} opt_a
 * @returns string
 * @constructor
 */
function String(opt_a) {}


// Predefined XPCOM runtime entities.
/** @constructor */
function XPCNativeWrapper(a) {}

var Components;
Components.classes;
Components.interfaces;
Components.utils;

var Application;
Application.console;
Application.console.log = function(arg) {};

function dump(text) {}

/** @constructor */
function nsICookie2(){}
/** @constructor */
function nsIFile(){}
/** @constructor */
function nsIIDRef(){}
/** @constructor */
function nsILocalFile(){}
/** @constructor */
function nsIDOMWindow(){}

/**
 * window.location pseudoclass, see
 * https://developer.mozilla.org/en/window.location#Location_object
 * @constructor
 */
function Location(){}
