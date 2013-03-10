// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
* Return the information of asynchronous script execution.
*
* @return {Object.<string, ?>} Information of asynchronous script execution.
*/
function getAsyncScriptInfo() {
  var key = 'chromedriverAsyncScriptInfo';
  if (!(key in document))
    document[key] = {'id': 0, 'finished': false};
  return document[key];
}

/**
* Execute the given script and save its asynchronous result.
*
* If script1 finishes after script2 is executed, then script1's result will be
* discarded while script2's will be saved.
*
* @param {!string} script The asynchronous script to be executed.
* @param {Array.<*>} args Arguments to be passed to the script.
*/
function executeAsyncScript(script, args) {
  var info = getAsyncScriptInfo();
  info.id++;
  info.finished = false;
  delete info.result;
  var id = info.id;

  function callback(result) {
    if (id == info.id) {
      info.result = result;
      info.finished = true;
    }
  }
  args.push(callback);

  try {
    new Function(script).apply(null, args);
  } catch (error) {
    error.code = 17;  // Error code for JavaScriptError.
    throw error;
  }
}
