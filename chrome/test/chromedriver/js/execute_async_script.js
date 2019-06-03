// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enum for WebDriver status codes.
 * @enum {number}
 */
var StatusCode = {
  OK: 0,
  UNKNOWN_ERROR: 13,
  JAVASCRIPT_ERROR: 17,
  SCRIPT_TIMEOUT: 28,
};


/**
* Execute the given script and return a promise containing its result.
*
* @param {string} script The asynchronous script to be executed. The script
*     should be a proper function body. It will be wrapped in a function and
*     invoked with the given arguments and, as the final argument, a callback
*     function to invoke to report the asynchronous result.
* @param {!Array<*>} args Arguments to be passed to the script.
* @param {boolean} isUserSupplied Whether the script is supplied by the user.
*     If not, UnknownError will be used instead of JavaScriptError if an
*     exception occurs during the script, and an additional error callback will
*     be supplied to the script.
*/
function executeAsyncScript(script, args, isUserSupplied) {
  function isThenable(value) {
    return typeof value === 'object' && typeof value.then === 'function';
  }
  let resolveHandle;
  let rejectHandle;
  var promise = new Promise((resolve, reject) => {
    resolveHandle = resolve;
    rejectHandle = reject;
  });

  args.push(resolveHandle);  // Append resolve to end of arguments list.
  if (!isUserSupplied)
    args.push(rejectHandle);

  // This confusing, round-about way accomplishing this script execution is to
  // follow the W3C execute-async-script spec.
  try {
    // The assumption is that each script is an asynchronous script.
    const scriptResult = new Function(script).apply(null, args);

    // First case is for user-scripts - they are all wrapped in an async
    // function in order to allow for "await" commands. As a result, all async
    // scripts from users will return a Promise that is thenable by default,
    // even if it doesn't return anything.
    if (isThenable(scriptResult)) {
      const resolvedPromise = Promise.resolve(scriptResult);
      resolvedPromise.then((value) => {
        // Must be thenable if user-supplied.
        if (!isUserSupplied || isThenable(value))
          resolveHandle(value);
      })
      .catch(rejectHandle);
    }
  } catch(error) {
    rejectHandle(error);
  }

  return promise.catch((error) => {
    const code = isUserSupplied ? StatusCode.JAVASCRIPT_ERROR :
                            (error.code || StatusCode.UNKNOWN_ERROR);
    error.code = code;
    throw error;
  });
}
