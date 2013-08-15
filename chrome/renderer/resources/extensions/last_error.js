// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  var errorMessage = name + ': ' + message;
  if (stack != null && stack != '')
    errorMessage += '\n' + stack;

  if (!targetChrome)
    throw new Error('No chrome object to set error: ' + errorMessage);
  clear(targetChrome);  // in case somebody has set a sneaky getter/setter

  var errorObject = { message: message };
  if (GetAvailability('extension.lastError').is_available)
    targetChrome.extension.lastError = errorObject;

  assertRuntimeIsAvailable();
  targetChrome.runtime.lastError = errorObject;
};

/**
 * Clears the last error on |targetChrome|.
 */
function clear(targetChrome) {
  if (!targetChrome)
    throw new Error('No target chrome to clear error');

  if (GetAvailability('extension.lastError').is_available)
    delete targetChrome.extension.lastError;

  assertRuntimeIsAvailable();
  delete targetChrome.runtime.lastError;
};

function assertRuntimeIsAvailable() {
  // chrome.runtime should always be available, but maybe it's disappeared for
  // some reason? Add debugging for http://crbug.com/258526.
  var runtimeAvailability = GetAvailability('runtime.lastError');
  if (!runtimeAvailability.is_available) {
    throw new Error('runtime.lastError is not available: ' +
                    runtimeAvailability.message);
  }
  if (!chrome.runtime)
    throw new Error('runtime namespace is null or undefined');
}

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
    $Function.apply(callback, undefined, args);
  } finally {
    clear(targetChrome);
  }
}

exports.clear = clear;
exports.set = set;
exports.run = run;
