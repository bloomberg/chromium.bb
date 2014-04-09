// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the fileSystemProvider API.

var binding = require('binding').Binding.create('fileSystemProvider');
var fileSystemProviderInternal =
    require('binding').Binding.create('fileSystemProviderInternal').generate();
var eventBindings = require('event_bindings');
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

  apiFunctions.setUpdateArgumentsPostValidate(
    'unmount',
    function(fileSystemId, successCallback, errorCallback) {
      // Piggyback the error callback onto the success callback,
      // so we can use the error callback later.
      successCallback.errorCallback_ = errorCallback;
      return [fileSystemId, successCallback];
    });

  apiFunctions.setCustomCallback(
    'unmount',
    function(name, request, response) {
      var domError = null;
      if (request.callback) {
        // DOMError is present only if mount() failed.
        if (response && response[0]) {
          // Convert a Dictionary to a DOMError.
          domError = GetDOMError(response[0].name, response[0].message);
          response.length = 1;
        }

        var successCallback = request.callback;
        var errorCallback = request.callback.errorCallback_;
        delete request.callback;

        if (domError)
          errorCallback(domError);
        else
          successCallback();
      }
    });
});

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onUnmountRequested',
    function(args, dispatch) {
      var fileSystemId = args[0];
      var requestId = args[1];
      var onSuccessCallback = function() {
        fileSystemProviderInternal.unmountRequestedSuccess(
            fileSystemId, requestId);
      };
      var onErrorCallback = function(error) {
        fileSystemProviderInternal.unmountRequestedError(
          fileSystemId, requestId, error);
      }
      dispatch([fileSystemId, onSuccessCallback, onErrorCallback]);
    });

exports.binding = binding.generate();
