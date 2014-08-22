// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var exceptionHandler = require('uncaught_exception_handler');
var lastError = require('lastError');
var logging = requireNative('logging');
var natives = requireNative('sendRequest');
var validate = require('schemaUtils').validate;

// All outstanding requests from sendRequest().
var requests = {};

// Used to prevent double Activity Logging for API calls that use both custom
// bindings and ExtensionFunctions (via sendRequest).
var calledSendRequest = false;

// Runs a user-supplied callback safely.
function safeCallbackApply(name, request, callback, args) {
  try {
    $Function.apply(callback, request, args);
  } catch (e) {
    exceptionHandler.handle('Error in response to ' + name, e, request.stack);
  }
}

// Callback handling.
function handleResponse(requestId, name, success, responseList, error) {
  // The chrome objects we will set lastError on. Really we should only be
  // setting this on the callback's chrome object, but set on ours too since
  // it's conceivable that something relies on that.
  var callerChrome = chrome;

  try {
    var request = requests[requestId];
    logging.DCHECK(request != null);

    // lastError needs to be set on the caller's chrome object no matter what,
    // though chances are it's the same as ours (it will be different when
    // calling API methods on other contexts).
    if (request.callback)
      callerChrome = natives.GetGlobal(request.callback).chrome;

    lastError.clear(chrome);
    if (callerChrome !== chrome)
      lastError.clear(callerChrome);

    if (!success) {
      if (!error)
        error = "Unknown error.";
      lastError.set(name, error, request.stack, chrome);
      if (callerChrome !== chrome)
        lastError.set(name, error, request.stack, callerChrome);
    }

    if (request.customCallback) {
      safeCallbackApply(name,
                        request,
                        request.customCallback,
                        $Array.concat([name, request], responseList));
    }

    if (request.callback) {
      // Validate callback in debug only -- and only when the
      // caller has provided a callback. Implementations of api
      // calls may not return data if they observe the caller
      // has not provided a callback.
      if (logging.DCHECK_IS_ON() && !error) {
        if (!request.callbackSchema.parameters)
          throw new Error(name + ": no callback schema defined");
        validate(responseList, request.callbackSchema.parameters);
      }
      safeCallbackApply(name, request, request.callback, responseList);
    }

    if (error &&
        !lastError.hasAccessed(chrome) &&
        !lastError.hasAccessed(callerChrome)) {
      // The native call caused an error, but the developer didn't check
      // runtime.lastError.
      // Notify the developer of the error via the (error) console.
      console.error("Unchecked runtime.lastError while running " +
          (name || "unknown") + ": " + error +
          (request.stack ? "\n" + request.stack : ""));
    }
  } finally {
    delete requests[requestId];
    lastError.clear(chrome);
    if (callerChrome !== chrome)
      lastError.clear(callerChrome);
  }
}

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
  request.stack = exceptionHandler.getExtensionStackTrace();
  if (optArgs.customCallback) {
    request.customCallback = optArgs.customCallback;
  }

  var nativeFunction = optArgs.nativeFunction || natives.StartRequest;

  var requestId = natives.GetNextRequestId();
  request.id = requestId;
  requests[requestId] = request;

  var hasCallback = request.callback || optArgs.customCallback;
  return nativeFunction(functionName,
                        request.args,
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
exports.safeCallbackApply = safeCallbackApply;

// Called by C++.
exports.handleResponse = handleResponse;
