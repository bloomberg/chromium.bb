// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var DCHECK = requireNative('logging').DCHECK;
var forEach = require('utils').forEach;
var json = require('json');
var lastError = require('lastError');
var natives = requireNative('sendRequest');
var validate = require('schemaUtils').validate;

// All outstanding requests from sendRequest().
var requests = {};

// Used to prevent double Activity Logging for API calls that use both custom
// bindings and ExtensionFunctions (via sendRequest).
var calledSendRequest = false;

// Callback handling.
chromeHidden.handleResponse = function(requestId, name,
                                       success, responseList, error) {
  // The chrome objects we will set lastError on. Really we should only be
  // setting this on the callback's chrome object, but set on ours too since
  // it's conceivable that something relies on that.
  var chromesForLastError = [chrome];

  try {
    var request = requests[requestId];
    DCHECK(request != null);

    // lastError needs to be set on the caller's chrome object no matter what,
    // though chances are it's the same as ours (it will be different when
    // calling API methods on other contexts).
    if (request.callback) {
      var chromeForCallback = natives.GetGlobal(request.callback).chrome;
      if (chromeForCallback != chrome)
        chromesForLastError.push(chromeForCallback);
    }

    forEach(chromesForLastError, function(i, c) {lastError.clear(c)});
    if (!success) {
      if (!error)
        error = "Unknown error.";
      forEach(chromesForLastError, function(i, c) {lastError.set(error, c)});
    }

    if (request.customCallback) {
      var customCallbackArgs = [name, request].concat(responseList);
      request.customCallback.apply(request, customCallbackArgs);
    }

    if (request.callback) {
      // Validate callback in debug only -- and only when the
      // caller has provided a callback. Implementations of api
      // calls may not return data if they observe the caller
      // has not provided a callback.
      if (chromeHidden.validateCallbacks && !error) {
        try {
          if (!request.callbackSchema.parameters) {
            throw new Error("No callback schemas defined");
          }

          validate(responseList, request.callbackSchema.parameters);
        } catch (exception) {
          return "Callback validation error during " + name + " -- " +
                 exception.stack;
        }
      }

      request.callback.apply(request, responseList);
    }
  } finally {
    delete requests[requestId];
    forEach(chromesForLastError, function(i, c) {lastError.clear(c)});
  }
};

function prepareRequest(args, argSchemas) {
  var request = {};
  var argCount = args.length;

  // Look for callback param.
  if (argSchemas.length > 0 &&
      argSchemas[argSchemas.length - 1].type == "function") {
    request.callback = args[args.length - 1];
    request.callbackSchema = argSchemas[argSchemas.length - 1];
    --argCount;
  }

  request.args = [];
  for (var k = 0; k < argCount; k++) {
    request.args[k] = args[k];
  }

  return request;
}

// Send an API request and optionally register a callback.
// |optArgs| is an object with optional parameters as follows:
// - noStringify: true if we should not stringify the request arguments.
// - customCallback: a callback that should be called instead of the standard
//   callback.
// - nativeFunction: the v8 native function to handle the request, or
//   StartRequest if missing.
// - forIOThread: true if this function should be handled on the browser IO
//   thread.
// - preserveNullInObjects: true if it is safe for null to be in objects.
function sendRequest(functionName, args, argSchemas, optArgs) {
  calledSendRequest = true;
  if (!optArgs)
    optArgs = {};
  var request = prepareRequest(args, argSchemas);
  if (optArgs.customCallback) {
    request.customCallback = optArgs.customCallback;
  }
  // json.stringify doesn't support a root object which is undefined.
  if (request.args === undefined)
    request.args = null;

  // TODO(asargent) - convert all optional native functions to accept raw
  // v8 values instead of expecting JSON strings.
  var doStringify = false;
  if (optArgs.nativeFunction && !optArgs.noStringify)
    doStringify = true;
  var requestArgs = doStringify ? json.stringify(request.args) : request.args;
  var nativeFunction = optArgs.nativeFunction || natives.StartRequest;

  var requestId = natives.GetNextRequestId();
  request.id = requestId;
  requests[requestId] = request;

  var hasCallback = request.callback || optArgs.customCallback;
  return nativeFunction(functionName,
                        requestArgs,
                        requestId,
                        hasCallback,
                        optArgs.forIOThread,
                        optArgs.preserveNullInObjects);
}

function getCalledSendRequest() {
  return calledSendRequest;
}

function clearCalledSendRequest() {
  calledSendRequest = false;
}

exports.sendRequest = sendRequest;
exports.getCalledSendRequest = getCalledSendRequest;
exports.clearCalledSendRequest = clearCalledSendRequest;
