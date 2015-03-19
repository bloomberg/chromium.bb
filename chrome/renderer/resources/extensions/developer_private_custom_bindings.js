// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the developerPrivate API.

var binding = require('binding').Binding.create('developerPrivate');

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  // Converts the argument of |functionName| from DirectoryEntry to URL.
  function bindFileSystemFunction(functionName) {
    apiFunctions.setUpdateArgumentsPostValidate(
        functionName, function(directoryEntry, callback) {
          var fileSystemName = directoryEntry.filesystem.name;
          var relativePath = $String.slice(directoryEntry.fullPath, 1);
          var url = directoryEntry.toURL();
          return [fileSystemName, relativePath, url, callback];
    });
  }

  bindFileSystemFunction('loadDirectory');

  // developerPrivate.enable is the same as chrome.management.setEnabled.
  // TODO(devlin): Migrate callers off developerPrivate.enable.
  bindingsAPI.compiledApi.enable = chrome.management.setEnabled;

  bindingsAPI.compiledApi.allowFileAccess = function(id, allow, callback) {
    chrome.developerPrivate.updateExtensionConfiguration(
        {extensionId: id, fileAccess: allow}, callback);
  };

  bindingsAPI.compiledApi.allowIncognito = function(id, allow, callback) {
    chrome.developerPrivate.updateExtensionConfiguration(
        {extensionId: id, incognitoAccess: allow}, callback);
  };
});

exports.binding = binding.generate();
