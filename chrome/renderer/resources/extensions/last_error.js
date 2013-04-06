// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DCHECK = requireNative('logging').DCHECK;
var GetAvailability = requireNative('v8_context').GetAvailability;
var GetGlobal = requireNative('sendRequest').GetGlobal;

// Utility for setting chrome.*.lastError.
//
// A utility here is useful for two reasons:
//  1. For backwards compatibility we need to set chrome.extension.lastError,
//     but not all contexts actually have access to the extension namespace.
//  2. When calling across contexts, the global object that gets lastError set
//     needs to be that of the caller. We force callers to explicitly specify
//     the chrome object to try to prevent bugs here.

/**
 * Sets the last error for |name| on |targetChrome| to |message| with an
 * optional |stack|.
 */
function set(name, message, stack, targetChrome) {
  DCHECK(targetChrome != undefined);
  clear(targetChrome);  // in case somebody has set a sneaky getter/setter

  var errorMessage = name + ': ' + message;
  if (stack != null && stack != '')
    errorMessage += '\n' + stack;
  console.error(errorMessage);

  var errorObject = { message: message };
  if (GetAvailability('extension').is_available)
    targetChrome.extension.lastError = errorObject;
  targetChrome.runtime.lastError = errorObject;
};

/**
 * Clears the last error on |targetChrome|.
 */
function clear(targetChrome) {
  DCHECK(targetChrome != undefined);
  if (GetAvailability('extension').is_available)
    delete targetChrome.extension.lastError;
  delete targetChrome.runtime.lastError;
};

/**
 * Runs |callback(args)| with last error args as in set().
 *
 * The target chrome object is the global object's of the callback, so this
 * method won't work if the real callback has been wrapped (etc).
 */
function run(name, message, stack, callback, args) {
  var targetChrome = GetGlobal(callback).chrome;
  set(name, message, stack, targetChrome);
  try {
    callback.apply(undefined, args);
  } finally {
    clear(targetChrome);
  }
}

exports.clear = clear;
exports.set = set;
exports.run = run;
