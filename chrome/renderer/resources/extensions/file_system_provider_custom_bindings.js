// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the fileSystemProvider API.

var binding = require('binding').Binding.create('fileSystemProvider');
var fileSystemNatives = requireNative('file_system_natives');
var GetDOMError = fileSystemNatives.GetDOMError;

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setUpdateArgumentsPostValidate(
    'mount',
    function(displayName, successCallback, errorCallback) {
      // Piggyback the error callback onto the success callback,
      // so we can use the error callback later.
      successCallback.errorCallback_ = errorCallback;
      return [displayName, successCallback];
    });

  apiFunctions.setCustomCallback(
    'mount',
    function(name, request, response) {
      var fileSystemId = null;
      var domError = null;
      if (request.callback && response) {
        fileSystemId = response[0];
        // DOMError is present only if mount() failed.
        if (response[1]) {
          // Convert a Dictionary to a DOMError.
          domError = GetDOMError(response[1].name, response[1].message);
          response.length = 1;
        }

        var successCallback = request.callback;
        var errorCallback = request.callback.errorCallback_;
        delete request.callback;

        if (domError)
          errorCallback(domError);
        else
          successCallback(fileSystemId);
      }
    });
});

exports.binding = binding.generate();
