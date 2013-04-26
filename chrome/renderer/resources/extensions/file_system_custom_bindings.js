// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the fileSystem API.

var binding = require('binding').Binding.create('fileSystem');

var fileSystemNatives = requireNative('file_system_natives');
var forEach = require('utils').forEach;
var GetIsolatedFileSystem = fileSystemNatives.GetIsolatedFileSystem;
var lastError = require('lastError');
var GetModuleSystem = requireNative('v8_context').GetModuleSystem;
// TODO(sammc): Don't require extension. See http://crbug.com/235689.
var GetExtensionViews = requireNative('extension').GetExtensionViews;

var backgroundPage = GetExtensionViews(-1, 'BACKGROUND')[0];
var backgroundPageModuleSystem = GetModuleSystem(backgroundPage);
var entryIdManager = backgroundPageModuleSystem.require('entryIdManager');

// All windows use the bindFileEntryCallback from the background page so their
// FileEntry objects have the background page's context as their own. This
// allows them to be used from other windows (including the background page)
// after the original window is closed.
if (window == backgroundPage) {
  var bindFileEntryCallback = function(functionName, apiFunctions) {
    apiFunctions.setCustomCallback(functionName,
        function(name, request, response) {
      if (request.callback && response) {
        var callback = request.callback;
        request.callback = null;

        var fileSystemId = response.fileSystemId;
        var baseName = response.baseName;
        var id = response.id;
        var fs = GetIsolatedFileSystem(fileSystemId);

        try {
          // TODO(koz): fs.root.getFile() makes a trip to the browser process,
          // but it might be possible avoid that by calling
          // WebFrame::createFileEntry().
          fs.root.getFile(baseName, {}, function(fileEntry) {
            entryIdManager.registerEntry(id, fileEntry);
            callback(fileEntry);
          }, function(fileError) {
            lastError.run('fileSystem.' + functionName,
                          'Error getting fileEntry, code: ' + fileError.code,
                          request.stack,
                          callback);
          });
        } catch (e) {
          lastError.run('fileSystem.' + functionName,
                        'Error in event handler for onLaunched: ' + e.stack,
                        request.stack,
                        callback);
        }
      }
    });
  };
} else {
  // Force the fileSystem API to be loaded in the background page. Using
  // backgroundPageModuleSystem.require('fileSystem') is insufficient as
  // requireNative is only allowed while lazily loading an API.
  backgroundPage.chrome.fileSystem;
  var bindFileEntryCallback = backgroundPageModuleSystem.require(
      'fileSystem').bindFileEntryCallback;
}

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;
  var fileSystem = bindingsAPI.compiledApi;

  function bindFileEntryFunction(i, functionName) {
    apiFunctions.setUpdateArgumentsPostValidate(
        functionName, function(fileEntry, callback) {
      var fileSystemName = fileEntry.filesystem.name;
      var relativePath = fileEntry.fullPath.slice(1);
      return [fileSystemName, relativePath, callback];
    });
  }
  forEach(['getDisplayPath', 'getWritableEntry', 'isWritableEntry'],
          bindFileEntryFunction);

  forEach(['getWritableEntry', 'chooseEntry'], function(i, functionName) {
    bindFileEntryCallback(functionName, apiFunctions);
  });

  apiFunctions.setHandleRequest('getEntryId', function(fileEntry) {
    return entryIdManager.getEntryId(fileEntry);
  });

  apiFunctions.setHandleRequest('getEntryById', function(id) {
    return entryIdManager.getEntryById(id);
  });

  // TODO(benwells): Remove these deprecated versions of the functions.
  fileSystem.getWritableFileEntry = function() {
    console.log("chrome.fileSystem.getWritableFileEntry is deprecated");
    console.log("Please use chrome.fileSystem.getWritableEntry instead");
    fileSystem.getWritableEntry.apply(this, arguments);
  };

  fileSystem.isWritableFileEntry = function() {
    console.log("chrome.fileSystem.isWritableFileEntry is deprecated");
    console.log("Please use chrome.fileSystem.isWritableEntry instead");
    fileSystem.isWritableEntry.apply(this, arguments);
  };

  fileSystem.chooseFile = function() {
    console.log("chrome.fileSystem.chooseFile is deprecated");
    console.log("Please use chrome.fileSystem.chooseEntry instead");
    fileSystem.chooseEntry.apply(this, arguments);
  };
});

exports.bindFileEntryCallback = bindFileEntryCallback;
exports.binding = binding.generate();
