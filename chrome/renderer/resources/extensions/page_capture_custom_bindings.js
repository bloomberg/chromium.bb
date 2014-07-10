// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the pageCapture API.

var binding = require('binding').Binding.create('pageCapture');

var handleUncaughtException = require('uncaught_exception_handler').handle;
var pageCaptureNatives = requireNative('page_capture');
var CreateBlob = pageCaptureNatives.CreateBlob;
var SendResponseAck = pageCaptureNatives.SendResponseAck;

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('saveAsMHTML',
                                 function(name, request, response) {
    var callback = request.callback;
    request.callback = null;
    if (response)
      response = CreateBlob(response.mhtmlFilePath, response.mhtmlFileLength);

    try {
      callback(response);
    } catch (e) {
      var message = "Error in chrome.pageCapture.saveAsMHTML callback: " + e;
      if (request.stack)
        message += "\n" + request.stack;
      handleUncaughtException(message, e);
    } finally {
      // Notify the browser. Now that the blob is referenced from JavaScript,
      // the browser can drop its reference to it.
      SendResponseAck(request.id);
    }
  });
});

exports.binding = binding.generate();
